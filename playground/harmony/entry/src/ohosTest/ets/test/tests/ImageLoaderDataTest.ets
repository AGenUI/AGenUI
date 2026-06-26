import { describe, it, expect, beforeEach } from '@ohos/hypium';
import {
  ImageLoaderErrorType,
  ImageFormat,
  ImageLoadOptionsKey,
  ImageLoaderConfiguration,
} from '@agenui/agenui';
import type { IImageLoader, ImageLoadOptions, ImageLoadResult, ImageLoaderError } from '@agenui/agenui';

class MockImageLoader implements IImageLoader {
  loadImage(
    url: string,
    options: ImageLoadOptions | null,
    onComplete: (requestId: string, result: ImageLoadResult | null, error: ImageLoaderError | null) => void
  ): string {
    return 'mock-request-id';
  }

  cancel(requestId: string): void {
    // no-op
  }

  clearCache(): void {
    // no-op
  }
}

export default function imageLoaderDataTest() {
  describe('ImageLoaderErrorType_enum', () => {

    it('invalidURL_hasCorrectValue', 0, () => {
      expect(ImageLoaderErrorType.INVALID_URL).assertEqual('invalidURL');
    });

    it('networkError_hasCorrectValue', 0, () => {
      expect(ImageLoaderErrorType.NETWORK_ERROR).assertEqual('networkError');
    });

    it('invalidData_hasCorrectValue', 0, () => {
      expect(ImageLoaderErrorType.INVALID_DATA).assertEqual('invalidData');
    });

    it('decompressionFailed_hasCorrectValue', 0, () => {
      expect(ImageLoaderErrorType.DECOMPRESSION_FAILED).assertEqual('decompressionFailed');
    });

    it('cancelled_hasCorrectValue', 0, () => {
      expect(ImageLoaderErrorType.CANCELLED).assertEqual('cancelled');
    });
  });

  describe('ImageFormat_enum', () => {

    it('unknown_hasCorrectValue', 0, () => {
      expect(ImageFormat.UNKNOWN).assertEqual('unknown');
    });

    it('jpeg_hasCorrectValue', 0, () => {
      expect(ImageFormat.JPEG).assertEqual('jpeg');
    });

    it('png_hasCorrectValue', 0, () => {
      expect(ImageFormat.PNG).assertEqual('png');
    });

    it('webp_hasCorrectValue', 0, () => {
      expect(ImageFormat.WEBP).assertEqual('webp');
    });

    it('gif_hasCorrectValue', 0, () => {
      expect(ImageFormat.GIF).assertEqual('gif');
    });
  });

  describe('ImageLoadOptionsKey_constants', () => {

    it('width_hasCorrectValue', 0, () => {
      expect(ImageLoadOptionsKey.width).assertEqual('width');
    });

    it('height_hasCorrectValue', 0, () => {
      expect(ImageLoadOptionsKey.height).assertEqual('height');
    });

    it('componentId_hasCorrectValue', 0, () => {
      expect(ImageLoadOptionsKey.componentId).assertEqual('componentId');
    });

    it('surfaceId_hasCorrectValue', 0, () => {
      expect(ImageLoadOptionsKey.surfaceId).assertEqual('surfaceId');
    });
  });

  describe('ImageLoaderConfiguration', () => {

    beforeEach(() => {
      // Reset loader state before each test
      ImageLoaderConfiguration.shared().loader = null;
    });

    it('shared_returnsSameInstance', 0, () => {
      const instance1 = ImageLoaderConfiguration.shared();
      const instance2 = ImageLoaderConfiguration.shared();
      expect(instance1).assertEqual(instance2);
    });

    it('hasLoader_initially_returnsFalse', 0, () => {
      const config = ImageLoaderConfiguration.shared();
      expect(config.hasLoader()).assertFalse();
    });

    it('setLoader_thenHasLoader_returnsTrue', 0, () => {
      const config = ImageLoaderConfiguration.shared();
      const mockLoader = new MockImageLoader();
      config.setLoader(mockLoader);
      expect(config.hasLoader()).assertTrue();
    });

    it('loader_afterSetNull_hasLoaderReturnsFalse', 0, () => {
      const config = ImageLoaderConfiguration.shared();
      const mockLoader = new MockImageLoader();
      config.setLoader(mockLoader);
      config.loader = null;
      expect(config.hasLoader()).assertFalse();
    });

    it('setLoader_loaderFieldAccessible', 0, () => {
      const config = ImageLoaderConfiguration.shared();
      const mockLoader = new MockImageLoader();
      config.setLoader(mockLoader);
      expect(config.loader).assertInstanceOf('MockImageLoader');
    });
  });
}
