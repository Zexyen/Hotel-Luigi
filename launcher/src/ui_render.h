// RmlUi's SDL GPU renderer, plus the pictures the screens use, drawn here instead of shipped as files: an image source
// that contains "gen:" names one (see ui_render.cpp for the list). The SDL GPU renderer draws no gradients, so the
// soft backgrounds, the slabs and the dial are textures.
#pragma once
#include "RmlUi_Renderer_SDL_GPU.h"

class UiRender : public RenderInterface_SDL_GPU {
public:
	using RenderInterface_SDL_GPU::RenderInterface_SDL_GPU;
	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
};
