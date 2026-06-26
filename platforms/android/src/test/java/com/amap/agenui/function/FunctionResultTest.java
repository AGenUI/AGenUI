package com.amap.agenui.function;

import org.json.JSONObject;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

public class FunctionResultTest {

    // ========================================================================
    // createSuccess
    // ========================================================================

    @Test
    public void createSuccess_stringData_statusIsSuccess() throws Exception {
        FunctionResult result = FunctionResult.createSuccess("hello");
        JSONObject json = result.toJson();
        assertEquals("Success", json.getString("status"));
        assertEquals("hello", json.get("data"));
    }

    @Test
    public void createSuccess_integerData_statusIsSuccess() throws Exception {
        FunctionResult result = FunctionResult.createSuccess(42);
        JSONObject json = result.toJson();
        assertEquals("Success", json.getString("status"));
        assertEquals(42, json.getInt("data"));
    }

    @Test
    public void createSuccess_booleanData_statusIsSuccess() throws Exception {
        FunctionResult result = FunctionResult.createSuccess(true);
        JSONObject json = result.toJson();
        assertEquals("Success", json.getString("status"));
        assertTrue(json.getBoolean("data"));
    }

    @Test
    public void createSuccess_nullData_noDataField() throws Exception {
        FunctionResult result = FunctionResult.createSuccess(null);
        JSONObject json = result.toJson();
        assertEquals("Success", json.getString("status"));
        assertTrue(!json.has("data") || json.isNull("data"));
    }

    @Test
    public void createSuccess_jsonObjectData_preservesStructure() throws Exception {
        JSONObject data = new JSONObject();
        data.put("key", "value");
        FunctionResult result = FunctionResult.createSuccess(data);
        JSONObject json = result.toJson();
        assertEquals("Success", json.getString("status"));
    }

    // ========================================================================
    // createError
    // ========================================================================

    @Test
    public void createError_withMessage_statusIsError() throws Exception {
        FunctionResult result = FunctionResult.createError("something broke");
        JSONObject json = result.toJson();
        assertEquals("Error", json.getString("status"));
        assertEquals("something broke", json.getString("error"));
    }

    @Test
    public void createError_nullMessage_errorFieldIsEmpty() throws Exception {
        FunctionResult result = FunctionResult.createError(null);
        JSONObject json = result.toJson();
        assertEquals("Error", json.getString("status"));
        assertEquals("", json.getString("error"));
    }

    @Test
    public void createError_emptyMessage_errorFieldIsEmpty() throws Exception {
        FunctionResult result = FunctionResult.createError("");
        JSONObject json = result.toJson();
        assertEquals("Error", json.getString("status"));
        assertEquals("", json.getString("error"));
    }

    // ========================================================================
    // createPending
    // ========================================================================

    @Test
    public void createPending_withRequestId_statusIsPending() throws Exception {
        FunctionResult result = FunctionResult.createPending("req-42");
        JSONObject json = result.toJson();
        assertEquals("Pending", json.getString("status"));
        assertEquals("req-42", json.getString("requestId"));
    }

    @Test
    public void createPending_nullRequestId_requestIdIsEmpty() throws Exception {
        FunctionResult result = FunctionResult.createPending(null);
        JSONObject json = result.toJson();
        assertEquals("Pending", json.getString("status"));
        assertEquals("", json.getString("requestId"));
    }

    // ========================================================================
    // toJsonString
    // ========================================================================

    @Test
    public void toJsonString_returnsValidJson() throws Exception {
        FunctionResult result = FunctionResult.createSuccess("data");
        String jsonStr = result.toJsonString();
        assertNotNull(jsonStr);
        JSONObject parsed = new JSONObject(jsonStr);
        assertEquals("Success", parsed.getString("status"));
    }

    @Test
    public void toJsonString_errorResult_parseable() throws Exception {
        FunctionResult result = FunctionResult.createError("fail");
        String jsonStr = result.toJsonString();
        JSONObject parsed = new JSONObject(jsonStr);
        assertEquals("Error", parsed.getString("status"));
        assertEquals("fail", parsed.getString("error"));
    }
}
