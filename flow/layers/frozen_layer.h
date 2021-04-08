// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FLOW_LAYERS_FROZEN_LAYER_H_
#define FLUTTER_FLOW_LAYERS_FROZEN_LAYER_H_

#include "flutter/flow/layers/container_layer.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"

namespace flutter {
    class FrozenTexture;

    class FrozenLayer : public ContainerLayer {
    public:
        FrozenLayer(int id, bool marker);

        ~FrozenLayer();

        void Preroll(PrerollContext *context, const SkMatrix &matrix) override;

        void Paint(PaintContext &context) const override;

    private:
        bool marker;
        int64_t texture_identifier;

        void TryInitSnapshot(PrerollContext *context, const SkMatrix &matrix);

        bool CheckTextureValid(PrerollContext *context, const FrozenTexture *frozenTexture, const SkMatrix &matrix);

        FML_DISALLOW_COPY_AND_ASSIGN(FrozenLayer);

    };

    class FrozenTexture : public Texture {
    public:
        struct ReleaseContext {
            bool destroyed;
            GrDirectContext *context;
            GrBackendTexture texture;
        };

        static void ReleaseTexture(void *ctx);

    public:
        FrozenTexture(int64_t texture_identifier, SkRect paint_bounds, SkIRect cache_rect, SkMatrix matrix,
                      sk_sp<SkSurface> backend_surface,
                      sk_sp<SkImage> cache_snapshot, std::shared_ptr<FrozenTexture::ReleaseContext> release_context,
                      bool marker);

        ~FrozenTexture();

        SkRect paint_bounds;
        SkIRect cache_rect;
        SkMatrix matrix;
        sk_sp<SkSurface> backend_surface;
        sk_sp<SkImage> cache_snapshot;
        std::shared_ptr<ReleaseContext> release_context;
        bool invalid;
        bool marker;
    private:
        void Paint(SkCanvas &canvas,
                   const SkRect &bounds,
                   bool freeze,
                   GrDirectContext *context,
                   SkFilterQuality filter_quality) override;

        void OnGrContextCreated() override;

        // |flutter::Texture|
        void OnGrContextDestroyed() override;

        // |flutter::Texture|
        void MarkNewFrameAvailable() override;

        // |flutter::Texture|
        void OnTextureUnregistered() override;

        FML_DISALLOW_COPY_AND_ASSIGN(FrozenTexture);

    };
}  // namespace flutter

#endif  // FLUTTER_FLOW_LAYERS_FROZEN_LAYER_H_
