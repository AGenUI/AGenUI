package com.amap.agenui.function;

import org.json.JSONObject;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

public class FunctionConfigTest {

    @Test
    public void getName_returnsConfiguredName() {
        FunctionConfig config = new FunctionConfig("testFunc");
        assertEquals("testFunc", config.getName());
    }

    @Test
    public void toJSON_containsNameField() throws Exception {
        FunctionConfig config = new FunctionConfig("myFunction");
        String jsonStr = config.toJSON();
        assertNotNull(jsonStr);
        JSONObject json = new JSONObject(jsonStr);
        assertEquals("myFunction", json.getString("name"));
    }

    @Test
    public void toJSON_emptyName_stillSerializes() throws Exception {
        FunctionConfig config = new FunctionConfig("");
        String jsonStr = config.toJSON();
        JSONObject json = new JSONObject(jsonStr);
        assertEquals("", json.getString("name"));
    }

    @Test
    public void toJSON_nullName_serializesAsNull() throws Exception {
        FunctionConfig config = new FunctionConfig(null);
        String jsonStr = config.toJSON();
        JSONObject json = new JSONObject(jsonStr);
        // JSONObject.put("name", null) serializes as JSONObject.NULL
        assertTrue_nameIsNullOrMissing(json);
    }

    private void assertTrue_nameIsNullOrMissing(JSONObject json) {
        // null name may be dropped or serialized as JSONObject.NULL
        if (json.has("name")) {
            assertEquals(JSONObject.NULL, json.opt("name"));
        }
    }
}
