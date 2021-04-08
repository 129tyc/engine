// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/layers/frozen_layer.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/src/gpu/GrResourceProvider.h"
#include "third_party/skia/src/gpu/GrTexture.h"
#include "third_party/skia/src/image/SkImage_Gpu.h"


namespace flutter {

    FrozenLayer::FrozenLayer(int id, bool marker) : marker(marker), texture_identifier(id) {}

    FrozenLayer::~FrozenLayer() = default;

    void FrozenLayer::Preroll(PrerollContext *context, const SkMatrix &matrix) {
        TRACE_EVENT0("flutter", "FrozenLayer::Preroll");


        auto existedTexture = context->texture_registry.GetTexture(texture_identifier);
        if (existedTexture == nullptr) {
            SkRect child_paint_bounds = SkRect::MakeEmpty();
            PrerollChildren(context, matrix, &child_paint_bounds);
            set_paint_bounds(child_paint_bounds);

            if (paint_bounds().isEmpty()) {
                return;
            }

            /*  FML_DLOG(ERROR)
              << "try create snapshot texture " << child_paint_bounds.x() << ":" << child_paint_bounds.y() << ":"
              << child_paint_bounds.width() << ":" << child_paint_bounds.height() << ":" << matrix.getTranslateX() << ":"
              << matrix.getTranslateY();
  */

            TryInitSnapshot(context, matrix);
        } else {
            auto frozenTexture = static_cast<FrozenTexture *>(existedTexture.get());
            FML_DLOG(ERROR)
            << "check re invalid, curr " << marker << " old " << frozenTexture->marker << " destroyed "
            << frozenTexture->release_context->destroyed << " translate " << matrix.getTranslateX() << " "
            << matrix.getTranslateY();

            if (frozenTexture->marker == marker || frozenTexture->release_context->destroyed) {
                SkRect child_paint_bounds = SkRect::MakeEmpty();
                PrerollChildren(context, matrix, &child_paint_bounds);
                set_paint_bounds(child_paint_bounds);

                if (paint_bounds().isEmpty()) {
                    return;
                }

                auto result = CheckTextureValid(context, frozenTexture, matrix);

                if (result) {
                    frozenTexture->marker = !marker;
                    frozenTexture->invalid = true;
                    frozenTexture->cache_rect = RasterCache::GetDeviceBounds(paint_bounds(), matrix);
                    frozenTexture->paint_bounds = paint_bounds();
                    frozenTexture->matrix = matrix;
                    FML_DLOG(ERROR) << "re invalid";
                }
            } else {
                frozenTexture->cache_rect = RasterCache::GetDeviceBounds(frozenTexture->paint_bounds, matrix);
                frozenTexture->matrix = matrix;
                set_paint_bounds(frozenTexture->paint_bounds);
            }
        }
    }

    bool FrozenLayer::CheckTextureValid(PrerollContext *context, const FrozenTexture *frozenTexture,
                                        const SkMatrix &matrix) {
        auto bounds = frozenTexture->cache_rect;
        auto rect = RasterCache::GetDeviceBounds(paint_bounds(), matrix);
        FML_DLOG(ERROR)
        << "existed texture size " << bounds.width() << " " << bounds.height() << " " << bounds.fLeft << " "
        << bounds.fTop;
        FML_DLOG(ERROR)
        << "check reuse texture " << rect.width() << " " << rect.height() << " " << rect.fLeft << " "
        << rect.fTop;
        if (bounds.width() != rect.width() || bounds.height() != rect.height() ||
            frozenTexture->release_context->destroyed) {
            FML_DLOG(ERROR) << "existed texture size not match or destroyed" << "unregister and re init";
//             flutter::FrozenTexture::ReleaseTexture(frozenTexture->release_context.get());
            context->texture_registry.UnregisterTexture(this->texture_identifier);

            TryInitSnapshot(context, matrix);
            return false;
        }
        return true;
    }

    void FrozenLayer::TryInitSnapshot(PrerollContext *context, const SkMatrix &matrix) {
        FML_DLOG(ERROR) << "try create snapshot texture";

        auto grContext = context->gr_context;

        auto colorspace = context->dst_color_space == nullptr ? nullptr : sk_sp<SkColorSpace>(context->dst_color_space);

        auto cache_rect = RasterCache::GetDeviceBounds(paint_bounds(), matrix);
        auto backendSurface = SkSurface::MakeRenderTarget(grContext, SkBudgeted::kYes,
                                                          SkImageInfo::Make(cache_rect.width(),
                                                                            cache_rect.height(),
                                                                            kN32_SkColorType,
                                                                            kPremul_SkAlphaType, colorspace));

        if (backendSurface == nullptr) {
            FML_DLOG(ERROR) << "create surface failed";
            return;
        }

        auto backendTexture = backendSurface->getBackendTexture(
                SkSurface::BackendHandleAccess::kFlushWrite_BackendHandleAccess);

        if (!backendTexture.isValid()) {
            FML_DLOG(ERROR) << "create texture failed";
            return;
        }

        auto api = backendTexture.backend();
        FML_DLOG(ERROR) << "texture backend api " << static_cast<unsigned int>(api);

        auto releaseContext = std::shared_ptr<FrozenTexture::ReleaseContext>(
                new FrozenTexture::ReleaseContext{false, grContext, backendTexture});

        auto snapshot = SkImage::MakeFromTexture(grContext, backendTexture,
                                                 kBottomLeft_GrSurfaceOrigin,
                                                 kN32_SkColorType,
                                                 kPremul_SkAlphaType,
                                                 colorspace, FrozenTexture::ReleaseTexture, releaseContext.get());

        if (!snapshot) {
            FML_DLOG(ERROR) << "new snapshot failed";
            return;
        }

        FML_DLOG(ERROR) << "snapshot create done";
        auto texture = std::make_shared<FrozenTexture>(texture_identifier,
                                                       paint_bounds(),
                                                       cache_rect,
                                                       matrix,
                                                       backendSurface, snapshot,
                                                       releaseContext, !marker);

        context->texture_registry.RegisterTexture(texture);

        FML_DLOG(ERROR) << "texture register done";
    }

    void FrozenLayer::Paint(PaintContext &context) const {
        TRACE_EVENT0("flutter", "FrozenLayer::Paint");
        FML_DCHECK(needs_painting());

        auto texture = context.texture_registry.GetTexture(texture_identifier);

        if (texture == nullptr) {
            FML_DLOG(ERROR) << "no cache snapshot texture, fallback";
            PaintChildren(context);
            return;
        }

        auto frozenTexture = static_cast<FrozenTexture *>(texture.get());
        if (frozenTexture->invalid) {
            FML_DLOG(ERROR) << "start invalid paint";

            auto tempInternalNode = context.internal_nodes_canvas;
            auto tempLeafNode = context.leaf_nodes_canvas;

            auto canvas = frozenTexture->backend_surface->getCanvas();
            auto canvas_size = canvas->getBaseLayerSize();

            canvas->resetMatrix();
            canvas->clear(SK_ColorTRANSPARENT);
            canvas->translate(-frozenTexture->cache_rect.fLeft, -frozenTexture->cache_rect.fTop);
            canvas->concat(frozenTexture->matrix);

            SkNWayCanvas internal_nodes_canvas(canvas_size.width(), canvas_size.height());
            internal_nodes_canvas.setMatrix(canvas->getTotalMatrix());
            internal_nodes_canvas.addCanvas(canvas);
            if (context.view_embedder != nullptr) {
                auto overlay_canvases = context.view_embedder->GetCurrentCanvases();
                for (auto &overlay_canvas : overlay_canvases) {
                    internal_nodes_canvas.addCanvas(overlay_canvas);
                }
            }

            context.leaf_nodes_canvas = canvas;
            context.internal_nodes_canvas = static_cast<SkCanvas *>(&internal_nodes_canvas);

            FML_DLOG(ERROR) << "paint children";
            PaintChildren(context);

            context.leaf_nodes_canvas = tempLeafNode;
            context.internal_nodes_canvas = tempInternalNode;
            frozenTexture->invalid = false;

            FML_DLOG(ERROR) << "draw children to cache";
        }

        SkAutoCanvasRestore auto_restore(context.leaf_nodes_canvas, true);
        context.leaf_nodes_canvas->resetMatrix();
        context.leaf_nodes_canvas->drawImage(frozenTexture->cache_snapshot, frozenTexture->cache_rect.fLeft,
                                             frozenTexture->cache_rect.fTop);
        FML_DLOG(ERROR)
        << "draw cache to leaf canvas " << frozenTexture->cache_rect.fLeft << " " << frozenTexture->cache_rect.fTop;
    }


    FrozenTexture::FrozenTexture(int64_t texture_identifier, SkRect paint_bounds, SkIRect cache_rect, SkMatrix matrix,
                                 sk_sp<SkSurface> backend_surface,
                                 sk_sp<SkImage> cache_snapshot,
                                 std::shared_ptr<FrozenTexture::ReleaseContext> release_context, bool marker)
            : Texture(texture_identifier), paint_bounds(paint_bounds), cache_rect(cache_rect), matrix(matrix),
              backend_surface(std::move(backend_surface)),
              cache_snapshot(std::move(cache_snapshot)),
              release_context(std::move(release_context)), invalid(true), marker(marker) {}

    FrozenTexture::~FrozenTexture() = default;

    void FrozenTexture::ReleaseTexture(void *ctx) {
        FML_DLOG(ERROR) << "try release texture";
        if (ctx == nullptr) {
            FML_DLOG(ERROR) << "ctx not existed";
            return;
        }
        FML_DLOG(ERROR) << "delete texture";
        auto releaseCtx = static_cast<FrozenTexture::ReleaseContext *>(ctx);
//        releaseCtx->context->deleteBackendTexture(releaseCtx->texture);
        releaseCtx->destroyed = true;
    }

    void FrozenTexture::Paint(SkCanvas &canvas, const SkRect &bounds, bool freeze, GrDirectContext *context,
                              SkFilterQuality filter_quality) {}

    void FrozenTexture::MarkNewFrameAvailable() {}

    void FrozenTexture::OnGrContextCreated() {
        FML_DLOG(ERROR) << "OnGrContextCreated";
    }

    void FrozenTexture::OnGrContextDestroyed() {
        FML_DLOG(ERROR) << "OnGrContextDestroyed";
        release_context->destroyed = true;
    }

    void FrozenTexture::OnTextureUnregistered() {
        FML_DLOG(ERROR) << "OnTextureUnregistered";
        release_context->destroyed = true;
    }

}  // namespace flutter
