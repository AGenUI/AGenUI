package com.amap.agenui.render.style;

import static org.junit.Assert.*;

import com.amap.a2ui_sdk.R;
import org.junit.Test;

/**
 * Unit tests for {@link IconResourceMapper}.
 * Verifies icon name -> resource ID mapping, case-insensitivity, null/unknown fallback.
 */
public class IconResourceMapperTest {

    // ─── Null & Unknown → default icon ────────────────────────────────────────

    @Test
    public void getIconResourceId_null_returnsDefault() {
        int result = IconResourceMapper.getIconResourceId(null);
        assertEquals(R.drawable.agenui_ic_circle_question_mark, result);
    }

    @Test
    public void getIconResourceId_unknownName_returnsDefault() {
        int result = IconResourceMapper.getIconResourceId("nonExistentIcon123");
        assertEquals(R.drawable.agenui_ic_circle_question_mark, result);
    }

    @Test
    public void getIconResourceId_emptyString_returnsDefault() {
        int result = IconResourceMapper.getIconResourceId("");
        assertEquals(R.drawable.agenui_ic_circle_question_mark, result);
    }

    // ─── Case insensitivity ──────────────────────────────────────────────────

    @Test
    public void getIconResourceId_caseInsensitive_lowercase() {
        int result = IconResourceMapper.getIconResourceId("add");
        assertEquals(R.drawable.agenui_ic_plus, result);
    }

    @Test
    public void getIconResourceId_caseInsensitive_uppercase() {
        int result = IconResourceMapper.getIconResourceId("ADD");
        assertEquals(R.drawable.agenui_ic_plus, result);
    }

    @Test
    public void getIconResourceId_caseInsensitive_mixedCase() {
        int result = IconResourceMapper.getIconResourceId("ArrowBack");
        assertEquals(R.drawable.agenui_ic_arrow_left, result);
    }

    // ─── Specific mappings ───────────────────────────────────────────────────

    @Test
    public void getIconResourceId_accountCircle() {
        assertEquals(R.drawable.agenui_ic_circle_user,
                IconResourceMapper.getIconResourceId("accountcircle"));
    }

    @Test
    public void getIconResourceId_arrowForward() {
        assertEquals(R.drawable.agenui_ic_arrow_right,
                IconResourceMapper.getIconResourceId("arrowforward"));
    }

    @Test
    public void getIconResourceId_attachFile() {
        assertEquals(R.drawable.agenui_ic_paperclip,
                IconResourceMapper.getIconResourceId("attachfile"));
    }

    @Test
    public void getIconResourceId_calendarToday() {
        assertEquals(R.drawable.agenui_ic_calendar,
                IconResourceMapper.getIconResourceId("calendartoday"));
    }

    @Test
    public void getIconResourceId_call() {
        assertEquals(R.drawable.agenui_ic_phone,
                IconResourceMapper.getIconResourceId("call"));
    }

    @Test
    public void getIconResourceId_camera() {
        assertEquals(R.drawable.agenui_ic_camera,
                IconResourceMapper.getIconResourceId("camera"));
    }

    @Test
    public void getIconResourceId_check() {
        assertEquals(R.drawable.agenui_ic_check,
                IconResourceMapper.getIconResourceId("check"));
    }

    @Test
    public void getIconResourceId_close() {
        assertEquals(R.drawable.agenui_ic_x,
                IconResourceMapper.getIconResourceId("close"));
    }

    @Test
    public void getIconResourceId_delete() {
        assertEquals(R.drawable.agenui_ic_trash,
                IconResourceMapper.getIconResourceId("delete"));
    }

    @Test
    public void getIconResourceId_download() {
        assertEquals(R.drawable.agenui_ic_download,
                IconResourceMapper.getIconResourceId("download"));
    }

    @Test
    public void getIconResourceId_edit() {
        assertEquals(R.drawable.agenui_ic_pencil,
                IconResourceMapper.getIconResourceId("edit"));
    }

    @Test
    public void getIconResourceId_event_sameAsCalendar() {
        assertEquals(R.drawable.agenui_ic_calendar,
                IconResourceMapper.getIconResourceId("event"));
    }

    @Test
    public void getIconResourceId_error() {
        assertEquals(R.drawable.agenui_ic_circle_alert,
                IconResourceMapper.getIconResourceId("error"));
    }

    @Test
    public void getIconResourceId_favorite() {
        assertEquals(R.drawable.agenui_ic_heart,
                IconResourceMapper.getIconResourceId("favorite"));
    }

    @Test
    public void getIconResourceId_favoriteOff() {
        assertEquals(R.drawable.agenui_ic_heart_off,
                IconResourceMapper.getIconResourceId("favoriteoff"));
    }

    @Test
    public void getIconResourceId_folder() {
        assertEquals(R.drawable.agenui_ic_folder,
                IconResourceMapper.getIconResourceId("folder"));
    }

    @Test
    public void getIconResourceId_help_sameAsDefault() {
        assertEquals(R.drawable.agenui_ic_circle_question_mark,
                IconResourceMapper.getIconResourceId("help"));
    }

    @Test
    public void getIconResourceId_home() {
        assertEquals(R.drawable.agenui_ic_house,
                IconResourceMapper.getIconResourceId("home"));
    }

    @Test
    public void getIconResourceId_info() {
        assertEquals(R.drawable.agenui_ic_info,
                IconResourceMapper.getIconResourceId("info"));
    }

    @Test
    public void getIconResourceId_locationOn() {
        assertEquals(R.drawable.agenui_ic_map_pin,
                IconResourceMapper.getIconResourceId("locationon"));
    }

    @Test
    public void getIconResourceId_lock() {
        assertEquals(R.drawable.agenui_ic_lock,
                IconResourceMapper.getIconResourceId("lock"));
    }

    @Test
    public void getIconResourceId_lockOpen() {
        assertEquals(R.drawable.agenui_ic_lock_open,
                IconResourceMapper.getIconResourceId("lockopen"));
    }

    @Test
    public void getIconResourceId_mail() {
        assertEquals(R.drawable.agenui_ic_mail,
                IconResourceMapper.getIconResourceId("mail"));
    }

    @Test
    public void getIconResourceId_menu() {
        assertEquals(R.drawable.agenui_ic_menu,
                IconResourceMapper.getIconResourceId("menu"));
    }

    @Test
    public void getIconResourceId_moreVert() {
        assertEquals(R.drawable.agenui_ic_ellipsis_vertical,
                IconResourceMapper.getIconResourceId("morevert"));
    }

    @Test
    public void getIconResourceId_moreHoriz() {
        assertEquals(R.drawable.agenui_ic_ellipsis,
                IconResourceMapper.getIconResourceId("morehoriz"));
    }

    @Test
    public void getIconResourceId_notifications() {
        assertEquals(R.drawable.agenui_ic_bell,
                IconResourceMapper.getIconResourceId("notifications"));
    }

    @Test
    public void getIconResourceId_notificationsOff() {
        assertEquals(R.drawable.agenui_ic_bell_off,
                IconResourceMapper.getIconResourceId("notificationsoff"));
    }

    @Test
    public void getIconResourceId_payment() {
        assertEquals(R.drawable.agenui_ic_credit_card,
                IconResourceMapper.getIconResourceId("payment"));
    }

    @Test
    public void getIconResourceId_person() {
        assertEquals(R.drawable.agenui_ic_user,
                IconResourceMapper.getIconResourceId("person"));
    }

    @Test
    public void getIconResourceId_phone() {
        assertEquals(R.drawable.agenui_ic_phone,
                IconResourceMapper.getIconResourceId("phone"));
    }

    @Test
    public void getIconResourceId_photo() {
        assertEquals(R.drawable.agenui_ic_image,
                IconResourceMapper.getIconResourceId("photo"));
    }

    @Test
    public void getIconResourceId_print() {
        assertEquals(R.drawable.agenui_ic_printer,
                IconResourceMapper.getIconResourceId("print"));
    }

    @Test
    public void getIconResourceId_refresh() {
        assertEquals(R.drawable.agenui_ic_refresh_cw,
                IconResourceMapper.getIconResourceId("refresh"));
    }

    @Test
    public void getIconResourceId_search() {
        assertEquals(R.drawable.agenui_ic_search,
                IconResourceMapper.getIconResourceId("search"));
    }

    @Test
    public void getIconResourceId_send() {
        assertEquals(R.drawable.agenui_ic_send,
                IconResourceMapper.getIconResourceId("send"));
    }

    @Test
    public void getIconResourceId_settings() {
        assertEquals(R.drawable.agenui_ic_settings,
                IconResourceMapper.getIconResourceId("settings"));
    }

    @Test
    public void getIconResourceId_share() {
        assertEquals(R.drawable.agenui_ic_share,
                IconResourceMapper.getIconResourceId("share"));
    }

    @Test
    public void getIconResourceId_shoppingCart() {
        assertEquals(R.drawable.agenui_ic_shopping_cart,
                IconResourceMapper.getIconResourceId("shoppingcart"));
    }

    @Test
    public void getIconResourceId_star() {
        assertEquals(R.drawable.agenui_ic_star,
                IconResourceMapper.getIconResourceId("star"));
    }

    @Test
    public void getIconResourceId_starHalf() {
        assertEquals(R.drawable.agenui_ic_star_half,
                IconResourceMapper.getIconResourceId("starhalf"));
    }

    @Test
    public void getIconResourceId_starOff() {
        assertEquals(R.drawable.agenui_ic_star_off,
                IconResourceMapper.getIconResourceId("staroff"));
    }

    @Test
    public void getIconResourceId_upload() {
        assertEquals(R.drawable.agenui_ic_upload,
                IconResourceMapper.getIconResourceId("upload"));
    }

    @Test
    public void getIconResourceId_visibility() {
        assertEquals(R.drawable.agenui_ic_eye,
                IconResourceMapper.getIconResourceId("visibility"));
    }

    @Test
    public void getIconResourceId_visibilityOff() {
        assertEquals(R.drawable.agenui_ic_eye_off,
                IconResourceMapper.getIconResourceId("visibilityoff"));
    }

    @Test
    public void getIconResourceId_warning() {
        assertEquals(R.drawable.agenui_ic_triangle_alert,
                IconResourceMapper.getIconResourceId("warning"));
    }

    // ─── Consistency checks ─────────────────────────────────────────────────

    @Test
    public void getIconResourceId_null_equalsUnknown() {
        // Both null and unknown should return the same default
        assertEquals(
                IconResourceMapper.getIconResourceId(null),
                IconResourceMapper.getIconResourceId("xyzNotAnIcon"));
    }

    @Test
    public void getIconResourceId_help_equalsDefault() {
        // "help" maps to the same icon as the default fallback
        assertEquals(
                IconResourceMapper.getIconResourceId(null),
                IconResourceMapper.getIconResourceId("help"));
    }

    @Test
    public void getIconResourceId_callAndPhone_sameResource() {
        // Both "call" and "phone" map to the phone icon
        assertEquals(
                IconResourceMapper.getIconResourceId("call"),
                IconResourceMapper.getIconResourceId("phone"));
    }

    @Test
    public void getIconResourceId_calendarTodayAndEvent_sameResource() {
        // Both "calendartoday" and "event" map to the calendar icon
        assertEquals(
                IconResourceMapper.getIconResourceId("calendartoday"),
                IconResourceMapper.getIconResourceId("event"));
    }
}
