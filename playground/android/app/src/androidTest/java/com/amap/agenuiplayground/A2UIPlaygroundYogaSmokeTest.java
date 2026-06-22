package com.amap.agenuiplayground;

import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.amap.agenui.render.layout.YogaAbsoluteLayout;
import com.amap.agenuiplayground.story.StoryLoader;
import com.amap.agenuiplayground.story.SubStory;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.lang.reflect.Method;
import java.util.List;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
public class A2UIPlaygroundYogaSmokeTest {

    @Rule
    public final ActivityScenarioRule<A2UIPlaygroundActivity> activityRule =
            new ActivityScenarioRule<>(A2UIPlaygroundActivity.class);

    @Test
    public void rowStoryRendersYogaAbsoluteLayout() throws Exception {
        activityRule.getScenario().onActivity(activity -> {
            StoryLoader storyLoader = new StoryLoader(activity);
            List<SubStory> stories = storyLoader.loadA2UIShowStories();
            SubStory rowStory = findStory(stories, "Row");
            assertNotNull("Row story should exist in playground assets", rowStory);

            try {
                Method loadSubStory = A2UIPlaygroundActivity.class
                        .getDeclaredMethod("loadSubStory", SubStory.class);
                loadSubStory.setAccessible(true);
                loadSubStory.invoke(activity, rowStory);
            } catch (ReflectiveOperationException e) {
                throw new AssertionError("Failed to invoke loadSubStory via reflection", e);
            }
        });

        SystemClock.sleep(2500L);

        activityRule.getScenario().onActivity(activity -> {
            FrameLayout renderContent = activity.findViewById(R.id.renderContent);
            assertNotNull("renderContent should be present", renderContent);
            assertTrue("renderContent should contain rendered views", renderContent.getChildCount() > 0);
            assertTrue("Rendered hierarchy should contain YogaAbsoluteLayout",
                    containsYogaAbsoluteLayout(renderContent));
        });
    }

    private static SubStory findStory(List<SubStory> stories, String subName) {
        if (stories == null) {
            return null;
        }
        for (SubStory story : stories) {
            if (story != null && subName.equals(story.getSubName())) {
                return story;
            }
        }
        return null;
    }

    private static boolean containsYogaAbsoluteLayout(View view) {
        if (view instanceof YogaAbsoluteLayout) {
            return true;
        }
        if (!(view instanceof ViewGroup)) {
            return false;
        }

        ViewGroup group = (ViewGroup) view;
        for (int i = 0; i < group.getChildCount(); i++) {
            if (containsYogaAbsoluteLayout(group.getChildAt(i))) {
                return true;
            }
        }
        return false;
    }
}
