package com.amap.agenuiplayground;

import android.os.Bundle;
import android.view.View;

import com.journeyapps.barcodescanner.CaptureActivity;
import com.journeyapps.barcodescanner.DecoratedBarcodeView;

/**
 * Portrait-mode barcode scanning Activity
 *
 * Extends CaptureActivity and configured as portrait orientation in AndroidManifest.xml.
 *
 * Uses a custom layout (activity_portrait_capture) that mirrors the default
 * zxing_capture layout and adds a close (X) button so the user can exit the
 * scanner without scanning, matching the system scanner behavior on HarmonyOS.
 */
public class PortraitCaptureActivity extends CaptureActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        View closeButton = findViewById(R.id.btn_close_scan);
        if (closeButton != null) {
            closeButton.setOnClickListener(v -> finish());
        }
    }

    @Override
    protected DecoratedBarcodeView initializeContent() {
        setContentView(R.layout.activity_portrait_capture);
        return (DecoratedBarcodeView) findViewById(R.id.zxing_barcode_scanner);
    }
}
