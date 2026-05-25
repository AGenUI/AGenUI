package com.amap.agenui.function;


import org.json.JSONException;
import org.json.JSONObject;
import com.amap.agenui.render.utils.AGenUILogger;

/**
 * Function execution result.
 *
 * Serialized to a JSON envelope crossing the JNI bridge to the C++ engine.
 * The schema matches the cross-platform protocol shared with iOS / Harmony:
 *   success: {"status":"Success","data":<value>}
 *   error:   {"status":"Error","error":"..."}
 *   pending: {"status":"Pending","requestId":"..."}
 */
public class FunctionResult {
    private static final String TAG = "FunctionResult";

    public enum Status { Success, Error, Pending }

    private Status status = Status.Error;
    private Object data;
    private String error;
    private String requestId;

    private FunctionResult() {}

    /**
     * Creates a success result.
     *
     * @param data Return value (String / JSONObject / JSONArray / Number / Boolean / null)
     */
    public static FunctionResult createSuccess(Object data) {
        FunctionResult r = new FunctionResult();
        r.status = Status.Success;
        r.data = data;
        return r;
    }

    /**
     * Creates an error result.
     *
     * @param error Human-readable error message
     */
    public static FunctionResult createError(String error) {
        FunctionResult r = new FunctionResult();
        r.status = Status.Error;
        r.error = error;
        return r;
    }

    /**
     * Creates a pending result for asynchronous functions.
     *
     * @param requestId Request id used to correlate the later async callback
     */
    public static FunctionResult createPending(String requestId) {
        FunctionResult r = new FunctionResult();
        r.status = Status.Pending;
        r.requestId = requestId;
        return r;
    }

    public JSONObject toJson() {
        JSONObject json = new JSONObject();
        try {
            json.put("status", status.name());
            switch (status) {
                case Success:
                    if (data != null) {
                        json.put("data", data);
                    }
                    break;
                case Error:
                    json.put("error", error != null ? error : "");
                    break;
                case Pending:
                    json.put("requestId", requestId != null ? requestId : "");
                    break;
            }
        } catch (JSONException e) {
            AGenUILogger.e(TAG, "toJson serialization failed", e);
        }
        return json;
    }

    public String toJsonString() {
        return toJson().toString();
    }
}
