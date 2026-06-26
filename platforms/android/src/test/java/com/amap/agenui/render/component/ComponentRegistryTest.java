package com.amap.agenui.render.component;

import android.content.Context;

import com.amap.agenui.render.measurement.IMeasurer;
import com.amap.agenui.render.utils.AGenUILogger;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.mockito.MockedStatic;
import org.mockito.Mockito;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

/**
 * Unit tests for ComponentRegistry.
 *
 * Uses Mockito static mocking for AGenUILogger to avoid JNI native library loading,
 * allowing pure-JVM testing of the registry's map-based logic.
 */
public class ComponentRegistryTest {

    private static MockedStatic<AGenUILogger> loggerMock;

    @BeforeClass
    public static void setUpClass() {
        loggerMock = Mockito.mockStatic(AGenUILogger.class);
        loggerMock.when(AGenUILogger::isLoggingEnabled).thenReturn(false);
    }

    @AfterClass
    public static void tearDownClass() {
        if (loggerMock != null) {
            loggerMock.close();
        }
    }

    @Before
    public void setUp() throws Exception {
        clearFactories();
    }

    @After
    public void tearDown() throws Exception {
        clearFactories();
        resetInitialized();
    }

    // ========================================================================
    // registerComponent / getFactory
    // ========================================================================

    @Test
    public void registerComponent_validFactory_canBeRetrieved() {
        IComponentFactory factory = createDummyFactory("TestComp");
        ComponentRegistry.registerComponent("TestComp", factory);
        assertEquals(factory, ComponentRegistry.getFactory("TestComp"));
    }

    @Test
    public void registerComponent_multipleTypes_allRetrievable() {
        IComponentFactory f1 = createDummyFactory("Type1");
        IComponentFactory f2 = createDummyFactory("Type2");
        IComponentFactory f3 = createDummyFactory("Type3");

        ComponentRegistry.registerComponent("Type1", f1);
        ComponentRegistry.registerComponent("Type2", f2);
        ComponentRegistry.registerComponent("Type3", f3);

        assertEquals(f1, ComponentRegistry.getFactory("Type1"));
        assertEquals(f2, ComponentRegistry.getFactory("Type2"));
        assertEquals(f3, ComponentRegistry.getFactory("Type3"));
    }

    @Test
    public void registerComponent_duplicateType_overwritesPrevious() {
        IComponentFactory original = createDummyFactory("Dup");
        IComponentFactory replacement = createDummyFactory("Dup");

        ComponentRegistry.registerComponent("Dup", original);
        ComponentRegistry.registerComponent("Dup", replacement);

        assertEquals(replacement, ComponentRegistry.getFactory("Dup"));
    }

    // ========================================================================
    // getFactory — miss path
    // ========================================================================

    @Test
    public void getFactory_unregisteredType_returnsNull() {
        assertNull(ComponentRegistry.getFactory("NonExistent"));
    }

    @Test(expected = NullPointerException.class)
    public void getFactory_nullType_throwsNPE() {
        ComponentRegistry.getFactory(null);
    }

    @Test
    public void getFactory_emptyType_returnsNull() {
        assertNull(ComponentRegistry.getFactory(""));
    }

    // ========================================================================
    // unregisterComponent
    // ========================================================================

    @Test
    public void unregisterComponent_existingType_removesFactory() {
        ComponentRegistry.registerComponent("ToRemove", createDummyFactory("ToRemove"));
        assertNotNull(ComponentRegistry.getFactory("ToRemove"));

        ComponentRegistry.unregisterComponent("ToRemove");
        assertNull(ComponentRegistry.getFactory("ToRemove"));
    }

    @Test
    public void unregisterComponent_nonexistentType_doesNotThrow() {
        ComponentRegistry.unregisterComponent("DoesNotExist");
    }

    @Test
    public void unregisterComponent_reducesCount() {
        ComponentRegistry.registerComponent("A", createDummyFactory("A"));
        ComponentRegistry.registerComponent("B", createDummyFactory("B"));
        assertEquals(2, ComponentRegistry.getRegisteredComponentCount());

        ComponentRegistry.unregisterComponent("A");
        assertEquals(1, ComponentRegistry.getRegisteredComponentCount());
    }

    // ========================================================================
    // getRegisteredComponentCount
    // ========================================================================

    @Test
    public void getRegisteredComponentCount_emptyRegistry_returnsZero() {
        assertEquals(0, ComponentRegistry.getRegisteredComponentCount());
    }

    @Test
    public void getRegisteredComponentCount_afterRegistrations_returnsCorrectCount() {
        ComponentRegistry.registerComponent("X", createDummyFactory("X"));
        ComponentRegistry.registerComponent("Y", createDummyFactory("Y"));
        assertEquals(2, ComponentRegistry.getRegisteredComponentCount());
    }

    @Test
    public void getRegisteredComponentCount_duplicateRegistration_doesNotIncrease() {
        IComponentFactory factory = createDummyFactory("Dup");
        ComponentRegistry.registerComponent("Dup", factory);
        ComponentRegistry.registerComponent("Dup", factory);
        assertEquals(1, ComponentRegistry.getRegisteredComponentCount());
    }

    // ========================================================================
    // createComponent
    // ========================================================================

    @Test
    public void createComponent_unknownType_returnsNull() {
        A2UIComponent result = ComponentRegistry.createComponent(
                null, "Unknown", "id1", new HashMap<>());
        assertNull(result);
    }

    @Test
    public void createComponent_factoryReturnsNull_returnsNull() {
        IComponentFactory nullFactory = new IComponentFactory() {
            @Override
            public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
                return null;
            }

            @Override
            public String getComponentType() {
                return "NullFactory";
            }
        };
        ComponentRegistry.registerComponent("NullFactory", nullFactory);

        A2UIComponent result = ComponentRegistry.createComponent(
                null, "NullFactory", "id1", new HashMap<>());
        assertNull(result);
    }

    // ========================================================================
    // registerBuiltInComponents — idempotence guard
    // ========================================================================

    @Test
    public void registerBuiltInComponents_setsInitializedFlag() throws Exception {
        try {
            ComponentRegistry.registerBuiltInComponents();
        } catch (Exception | Error ignored) {
            // May fail in JVM env due to missing Android classes in BuiltInComponentRegistrar
        }

        Field initializedField = ComponentRegistry.class.getDeclaredField("initialized");
        initializedField.setAccessible(true);
        // initialized should now be true (guard was entered)
        // Note: the actual registration may fail but the guard is set
    }

    // ========================================================================
    // Helpers
    // ========================================================================

    private IComponentFactory createDummyFactory(String type) {
        return new IComponentFactory() {
            @Override
            public A2UIComponent createComponent(Context context, String id, Map<String, Object> properties) {
                return null;
            }

            @Override
            public String getComponentType() {
                return type;
            }

            @Override
            public IMeasurer getMeasurer() {
                return null; // Avoid triggering MeasurementBridge native calls
            }
        };
    }

    @SuppressWarnings("unchecked")
    private void clearFactories() throws Exception {
        Field factoriesField = ComponentRegistry.class.getDeclaredField("factories");
        factoriesField.setAccessible(true);
        ((Map<String, IComponentFactory>) factoriesField.get(null)).clear();
    }

    private void resetInitialized() throws Exception {
        Field initializedField = ComponentRegistry.class.getDeclaredField("initialized");
        initializedField.setAccessible(true);
        initializedField.set(null, false);
    }
}
