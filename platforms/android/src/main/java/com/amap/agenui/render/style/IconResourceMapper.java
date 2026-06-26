package com.amap.agenui.render.style;

import com.amap.a2ui_sdk.R;

/**
 * Maps A2UI v0.9 standard icon names to Android drawable resource IDs (Lucide Icons).
 * <p>
 * Downstream integrations that exclude icon resources (res-icons) should also exclude
 * this file via build.gradle to avoid unresolved R.drawable references.
 */
public class IconResourceMapper {

    private IconResourceMapper() {
    }

    /**
     * Returns the resource ID for a standard icon.
     * <p>
     * Maps A2UI v0.9 standard icons to Lucide Icons (46 high-quality SVG icons),
     * listed in the order defined by the A2UI catalog.
     * <p>
     * Note: the following 11 media control icons are not implemented:
     * fastForward, pause, play, rewind, skipNext, skipPrevious, stop,
     * volumeDown, volumeMute, volumeOff, volumeUp.
     *
     * @param iconName Icon name (case-insensitive)
     * @return Icon resource ID; returns the default icon if not found
     */
    public static int getIconResourceId(String iconName) {
        if (iconName == null) {
            return R.drawable.agenui_ic_circle_question_mark;
        }

        switch (iconName.toLowerCase()) {
            case "accountcircle":
                return R.drawable.agenui_ic_circle_user;
            case "add":
                return R.drawable.agenui_ic_plus;
            case "arrowback":
                return R.drawable.agenui_ic_arrow_left;
            case "arrowforward":
                return R.drawable.agenui_ic_arrow_right;
            case "attachfile":
                return R.drawable.agenui_ic_paperclip;
            case "calendartoday":
                return R.drawable.agenui_ic_calendar;
            case "call":
                return R.drawable.agenui_ic_phone;
            case "camera":
                return R.drawable.agenui_ic_camera;
            case "check":
                return R.drawable.agenui_ic_check;
            case "close":
                return R.drawable.agenui_ic_x;
            case "delete":
                return R.drawable.agenui_ic_trash;
            case "download":
                return R.drawable.agenui_ic_download;
            case "edit":
                return R.drawable.agenui_ic_pencil;
            case "event":
                return R.drawable.agenui_ic_calendar;
            case "error":
                return R.drawable.agenui_ic_circle_alert;
            case "favorite":
                return R.drawable.agenui_ic_heart;
            case "favoriteoff":
                return R.drawable.agenui_ic_heart_off;
            case "folder":
                return R.drawable.agenui_ic_folder;
            case "help":
                return R.drawable.agenui_ic_circle_question_mark;
            case "home":
                return R.drawable.agenui_ic_house;
            case "info":
                return R.drawable.agenui_ic_info;
            case "locationon":
                return R.drawable.agenui_ic_map_pin;
            case "lock":
                return R.drawable.agenui_ic_lock;
            case "lockopen":
                return R.drawable.agenui_ic_lock_open;
            case "mail":
                return R.drawable.agenui_ic_mail;
            case "menu":
                return R.drawable.agenui_ic_menu;
            case "morevert":
                return R.drawable.agenui_ic_ellipsis_vertical;
            case "morehoriz":
                return R.drawable.agenui_ic_ellipsis;
            case "notificationsoff":
                return R.drawable.agenui_ic_bell_off;
            case "notifications":
                return R.drawable.agenui_ic_bell;
            case "payment":
                return R.drawable.agenui_ic_credit_card;
            case "person":
                return R.drawable.agenui_ic_user;
            case "phone":
                return R.drawable.agenui_ic_phone;
            case "photo":
                return R.drawable.agenui_ic_image;
            case "print":
                return R.drawable.agenui_ic_printer;
            case "refresh":
                return R.drawable.agenui_ic_refresh_cw;
            case "search":
                return R.drawable.agenui_ic_search;
            case "send":
                return R.drawable.agenui_ic_send;
            case "settings":
                return R.drawable.agenui_ic_settings;
            case "share":
                return R.drawable.agenui_ic_share;
            case "shoppingcart":
                return R.drawable.agenui_ic_shopping_cart;
            case "star":
                return R.drawable.agenui_ic_star;
            case "starhalf":
                return R.drawable.agenui_ic_star_half;
            case "staroff":
                return R.drawable.agenui_ic_star_off;
            case "upload":
                return R.drawable.agenui_ic_upload;
            case "visibility":
                return R.drawable.agenui_ic_eye;
            case "visibilityoff":
                return R.drawable.agenui_ic_eye_off;
            case "warning":
                return R.drawable.agenui_ic_triangle_alert;
            default:
                return R.drawable.agenui_ic_circle_question_mark;
        }
    }
}
