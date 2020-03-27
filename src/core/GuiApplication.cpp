// Copyright © 2008-2020 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "GuiApplication.h"
#include "GameConfig.h"
#include "OS.h"

#include "SDL.h"
#include "graphics/Drawables.h"
#include "graphics/Graphics.h"
#include "graphics/RenderState.h"
#include "graphics/RenderTarget.h"
#include "graphics/Renderer.h"
#include "graphics/Texture.h"
#include "utils.h"
#include "versioningInfo.h"

// FIXME: add support for offscreen rendertarget drawing and multisample RTs
#define RTT 0

void GuiApplication::BeginFrame()
{
#if RTT
	m_renderer->SetRenderTarget(m_renderTarget);
#endif
	// TODO: render target size
	m_renderer->SetViewport(0, 0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight());
	m_renderer->BeginFrame();
}

void GuiApplication::DrawRenderTarget()
{
#if RTT
	m_renderer->BeginFrame();
	m_renderer->SetViewport(0, 0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight());
	m_renderer->SetTransform(matrix4x4f::Identity());

	{
		m_renderer->SetMatrixMode(Graphics::MatrixMode::PROJECTION);
		m_renderer->PushMatrix();
		m_renderer->SetOrthographicProjection(0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), 0, -1, 1);
		m_renderer->SetMatrixMode(Graphics::MatrixMode::MODELVIEW);
		m_renderer->PushMatrix();
		m_renderer->LoadIdentity();
	}

	m_renderQuad->Draw(m_renderer);

	{
		m_renderer->SetMatrixMode(Graphics::MatrixMode::PROJECTION);
		m_renderer->PopMatrix();
		m_renderer->SetMatrixMode(Graphics::MatrixMode::MODELVIEW);
		m_renderer->PopMatrix();
	}

	m_renderer->EndFrame();
#endif
}

void GuiApplication::EndFrame()
{
#if RTT
	m_renderer->SetRenderTarget(nullptr);
	DrawRenderTarget();
#endif
	m_renderer->EndFrame();
	m_renderer->SwapBuffers();
}

Graphics::RenderTarget *GuiApplication::CreateRenderTarget(const Graphics::Settings &settings)
{
	/*	@fluffyfreak here's a rendertarget implementation you can use for oculusing and other things. It's pretty simple:
		 - fill out a RenderTargetDesc struct and call Renderer::CreateRenderTarget
		 - pass target to Renderer::SetRenderTarget to start rendering to texture
		 - set up viewport, clear etc, then draw as usual
		 - SetRenderTarget(0) to resume render to screen
		 - you can access the attached texture with GetColorTexture to use it with a material
		You can reuse the same target with multiple textures.
		In that case, leave the color format to NONE so the initial texture is not created, then use SetColorTexture to attach your own.
	*/
#if RTT
	Graphics::RenderStateDesc rsd;
	rsd.depthTest = false;
	rsd.depthWrite = false;
	rsd.blendMode = Graphics::BLEND_SOLID;
	m_renderState.reset(m_renderer->CreateRenderState(rsd));

	// Complete the RT description so we can request a buffer.
	Graphics::RenderTargetDesc rtDesc(
		width,
		height,
		Graphics::TEXTURE_RGB_888, // don't create a texture
		Graphics::TEXTURE_DEPTH,
		false, settings.requestedSamples);
	m_renderTarget.reset(m_renderer->CreateRenderTarget(rtDesc));

	m_renderTarget->SetColorTexture(*m_renderTexture);
#endif

	return nullptr;
}

Graphics::Renderer *GuiApplication::StartupRenderer(const GameConfig *config, bool hidden)
{
	PROFILE_SCOPED()
	// Initialize SDL
	Uint32 sdlInitFlags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
	if (SDL_Init(sdlInitFlags) < 0) {
		Error("SDL initialization failed: %s\n", SDL_GetError());
	}

	OutputVersioningInfo();

	// determine what renderer we should use, default to Opengl 3.x
	const std::string rendererName = config->String("RendererName", Graphics::RendererNameFromType(Graphics::RENDERER_OPENGL_3x));
	// if we add new renderer types, make sure to update this logic
	Graphics::RendererType rType = Graphics::RENDERER_OPENGL_3x;

	Graphics::Settings videoSettings = {};
	videoSettings.rendererType = rType;
	videoSettings.width = config->Int("ScrWidth");
	videoSettings.height = config->Int("ScrHeight");
	videoSettings.fullscreen = (config->Int("StartFullscreen") != 0);
	videoSettings.hidden = hidden;
	videoSettings.requestedSamples = config->Int("AntiAliasingMode");
	videoSettings.vsync = (config->Int("VSync") != 0);
	videoSettings.useTextureCompression = (config->Int("UseTextureCompression") != 0);
	videoSettings.useAnisotropicFiltering = (config->Int("UseAnisotropicFiltering") != 0);
	videoSettings.enableDebugMessages = (config->Int("EnableGLDebug") != 0);
	videoSettings.gl3ForwardCompatible = (config->Int("GL3ForwardCompatible") != 0);
	videoSettings.iconFile = OS::GetIconFilename();
	videoSettings.title = m_applicationTitle.c_str();

	m_renderer.reset(Graphics::Init(videoSettings));
	m_renderTarget.reset(CreateRenderTarget(videoSettings));

	return m_renderer.get();
}

void GuiApplication::ShutdownRenderer()
{
	m_renderTarget.reset();
	m_renderState.reset();
	m_renderer.reset();

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
