// Copyright(c) 2019 - 2020, #Momo
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
// 
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and /or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "Texture.h"
#include "Platform/OpenGL/GLUtilities.h"
#include "Utilities/Logging/Logger.h"
#include "Utilities/Time/Time.h"
#include "Utilities/Image/ImageLoader.h"
#include "Utilities/FileSystem/File.h"

namespace MxEngine
{
	GLenum formatTable[] =
	{
		GL_R8,
		GL_R16,
		GL_RG8,
		GL_RG16,
		GL_R16F,
		GL_R32F,
		GL_RG16F,
		GL_RG32F,
		GL_RGB,
		GL_RGBA,
		GL_RGB16,
		GL_RGB16F,
		GL_RGBA16,
		GL_RGBA16F,
		GL_RGB32F,
		GL_RGBA32F,
		GL_DEPTH_COMPONENT,
		GL_DEPTH_COMPONENT32F
	};

	GLenum wrapTable[] =
	{
		GL_CLAMP_TO_EDGE,
		GL_CLAMP_TO_BORDER,
		GL_MIRRORED_REPEAT,
		GL_REPEAT,
	};

	void Texture::FreeTexture()
	{
		if (id != 0)
		{
			GLCALL(glDeleteTextures(1, &id));
		}
		id = 0;
		activeId = 0;
	}

	Texture::Texture()
	{
		GLCALL(glGenTextures(1, &id));
		MXLOG_DEBUG("OpenGL::Texture", "created texture with id = " + ToMxString(id));
	}

	Texture::Texture(Texture&& texture) noexcept
	{
		this->width = texture.width;
		this->height = texture.height;
		this->textureType = texture.textureType;
		this->filepath = std::move(texture.filepath);
		this->wrapType = texture.wrapType;
		this->samples = texture.samples;
		this->format = texture.format;
		this->id = texture.id;

		texture.id = 0;
		texture.activeId = 0;
		texture.width = 0;
		texture.height = 0;
		texture.filepath = "[[deleted]]";
		texture.samples = 0;
	}

	Texture& Texture::operator=(Texture&& texture) noexcept
	{
		this->FreeTexture();

		this->width = texture.width;
		this->height = texture.height;
		this->textureType = texture.textureType;
		this->filepath = std::move(texture.filepath);
		this->wrapType = texture.wrapType;
		this->samples = texture.samples;
		this->format = texture.format;
		this->id = texture.id;
		
		texture.id = 0;
		texture.activeId = 0;
		texture.width = 0;
		texture.height = 0;
		texture.filepath = "[[deleted]]";
		texture.samples = 0;

		return *this;
	}

	Texture::~Texture()
	{
		this->FreeTexture();
	}

	template<>
	void Texture::Load(const std::filesystem::path& filepath, TextureFormat format, TextureWrap wrap, bool genMipmaps, bool flipImage)
	{
		// TODO: support floating point texture loading
		Image image = ImageLoader::LoadImage(filepath, flipImage);

		if (image.GetRawData() == nullptr)
		{
			MXLOG_ERROR("Texture", "file with name '" + ToMxString(filepath) + "' was not found");
			return;
		}

		this->filepath = ToMxString(std::filesystem::proximate(filepath));
		this->wrapType = wrap;
		this->format = format;
		this->width = image.GetWidth();
		this->height = image.GetHeight();
		this->textureType = GL_TEXTURE_2D;

		size_t channels = image.GetChannelCount();
		GLenum pixelType = image.IsFloatingPoint() ? GL_FLOAT : GL_UNSIGNED_BYTE;
		GLenum pixelFormat = GL_RGBA;
		switch (channels)
		{
		case 1:
			pixelFormat = GL_RED;
			break;
		case 2:
			pixelFormat = GL_RG;
			break;
		case 3:
			pixelFormat = GL_RGB;
			break;
		case 4:
			pixelFormat = GL_RGBA;
			break;
		default:
			MXLOG_ERROR("OpenGL::Texture", "invalid channel count: " + ToMxString(channels));
			break;
		}

		GLCALL(glBindTexture(GL_TEXTURE_2D, id));
		GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, formatTable[(int)this->format], (GLsizei)width, (GLsizei)height, 0, pixelFormat, pixelType, image.GetRawData()));

		GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapTable[(int)this->wrapType]));
		GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapTable[(int)this->wrapType]));
		
		if (genMipmaps) this->GenerateMipmaps();
	}

	template<>
	Texture::Texture(const std::filesystem::path& filepath, TextureFormat format, TextureWrap wrap, bool genMipmaps, bool flipImage)
		: Texture()
	{
		this->Load(filepath, format, wrap, genMipmaps, flipImage);
	}

	void Texture::Load(RawDataPointer data, int width, int height, int channels, bool isFloating, TextureFormat format, TextureWrap wrap, bool genMipmaps)
	{
		this->filepath = "[[raw data]]";
		this->width = width;
		this->height = height;
		this->textureType = GL_TEXTURE_2D;
		this->format = format;
		this->wrapType = wrap;

		GLenum type = isFloating ? GL_FLOAT : GL_UNSIGNED_BYTE;

		GLenum dataChannels = GL_RGB;
		switch (channels)
		{
		case 1:
			dataChannels = GL_RED;
			break;
		case 2:
			dataChannels = GL_RG;
			break;
		case 3:
			dataChannels = GL_RGB;
			break;
		case 4:
			dataChannels = GL_RGBA;
			break;
		default:
			MXLOG_ERROR("OpenGL::Texture", "invalid channel count: " + ToMxString(channels));
			break;
		}

		GLCALL(glBindTexture(GL_TEXTURE_2D, id));
		GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, formatTable[(int)this->format], (GLsizei)width, (GLsizei)height, 0, dataChannels, type, data));

		GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapTable[(int)this->wrapType]));
		GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapTable[(int)this->wrapType]));

		if (genMipmaps) this->GenerateMipmaps();
	}

    void Texture::Load(const Image& image, TextureFormat format, TextureWrap wrap, bool genMipmaps)
    {
		this->Load(image.GetRawData(), (int)image.GetWidth(), (int)image.GetHeight(), (int)image.GetChannelCount(), image.IsFloatingPoint(), format, wrap, genMipmaps);
    }

	void Texture::LoadDepth(int width, int height, TextureFormat format, TextureWrap wrap)
	{
		this->filepath = "[[depth]]";
		this->width = width;
		this->height = height;
		this->textureType = GL_TEXTURE_2D;
		this->format = format;
		this->wrapType = wrap;

		this->Bind();

		GLenum type = this->IsFloatingPoint() ? GL_FLOAT : GL_UNSIGNED_BYTE;

		GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, formatTable[(int)this->format], width, height, 0, GL_DEPTH_COMPONENT, type, nullptr));
		GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapTable[(int)this->wrapType]));
		GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapTable[(int)this->wrapType]));

		this->SetBorderColor(MakeVector4(1.0f));
	}

	void Texture::SetSamplingFromLOD(size_t lod)
	{
		this->Bind();
		GLCALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, (float)lod));
		GLCALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, (float)lod));
	}

	size_t Texture::GetMaxTextureLOD() const
	{
		return Log2(Max(this->width, this->height));
	}

	Image Texture::GetRawTextureData() const
	{
		if (this->height == 0 || this->width == 0)
			return Image();

		GLenum type = this->IsFloatingPoint() ? GL_FLOAT : GL_UNSIGNED_BYTE;
		size_t pixelSize = this->GetChannelCount() * (this->IsFloatingPoint() ? sizeof(float) : sizeof(uint8_t));
		size_t totalByteSize = this->width * this->height * pixelSize;
		auto result = (uint8_t*)std::malloc(totalByteSize);

		GLenum readFormat = GL_RGBA;
		switch (this->GetChannelCount())
		{
		case 1:
			readFormat = GL_RED;
			break;
		case 2:
			readFormat = GL_RG;
			break;
		case 3:
			readFormat = GL_RGB;
			break;
		case 4:
			readFormat = GL_RGBA;
			break;
		default:
			MXLOG_ERROR("OpenGL::Texture", "invalid channel count: " + ToMxString(this->GetChannelCount()));
			break;
		}

		this->Bind(0);
		GLCALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
		GLCALL(glGetTexImage(this->textureType, 0, readFormat, type, (void*)result));
		return Image(result, this->width, this->height, this->GetChannelCount(), this->IsFloatingPoint());
    }

	void Texture::GenerateMipmaps()
	{
		this->Bind(0);
		GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
		GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GLCALL(glGenerateMipmap(GL_TEXTURE_2D));
	}

    void Texture::SetBorderColor(const Vector3& color)
    {
		this->Bind(0);
		auto normalized = Clamp(color, MakeVector3(0.0f), MakeVector3(1.0f));
		GLCALL(glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &normalized[0]));
    }
	
    bool Texture::IsMultisampled() const
    {
		return this->textureType == GL_TEXTURE_2D_MULTISAMPLE;
    }

	bool Texture::IsFloatingPoint() const
	{
		switch (this->format)
		{
		case MxEngine::TextureFormat::R:
			return false;
		case MxEngine::TextureFormat::R16:
			return false;
		case MxEngine::TextureFormat::RG:
			return false;
		case MxEngine::TextureFormat::RG16:
			return false;
		case MxEngine::TextureFormat::R16F:
			return true;
		case MxEngine::TextureFormat::R32F:
			return true;
		case MxEngine::TextureFormat::RG16F:
			return true;
		case MxEngine::TextureFormat::RG32F:
			return true;
		case MxEngine::TextureFormat::RGB:
			return false;
		case MxEngine::TextureFormat::RGBA:
			return false;
		case MxEngine::TextureFormat::RGB16:
			return false;
		case MxEngine::TextureFormat::RGB16F:
			return true;
		case MxEngine::TextureFormat::RGBA16:
			return false;
		case MxEngine::TextureFormat::RGBA16F:
			return true;
		case MxEngine::TextureFormat::RGB32F:
			return true;
		case MxEngine::TextureFormat::RGBA32F:
			return true;
		case MxEngine::TextureFormat::DEPTH:
			return false;
		case MxEngine::TextureFormat::DEPTH32F:
			return true;
		default:
			return false;
		}
	}

	bool Texture::IsDepthOnly() const
	{
		return format == TextureFormat::DEPTH || this->format == TextureFormat::DEPTH32F;
	}

    int Texture::GetSampleCount() const
    {
		return (int)this->samples;
    }

	size_t Texture::GetPixelSize() const
	{
		switch (this->format)
		{
		case MxEngine::TextureFormat::R:
			return 1;
		case MxEngine::TextureFormat::R16:
			return 2;
		case MxEngine::TextureFormat::RG:
			return 2;
		case MxEngine::TextureFormat::RG16:
			return 4;
		case MxEngine::TextureFormat::R16F:
			return 2;
		case MxEngine::TextureFormat::R32F:
			return 4;
		case MxEngine::TextureFormat::RG16F:
			return 4;
		case MxEngine::TextureFormat::RG32F:
			return 8;
		case MxEngine::TextureFormat::RGB:
			return 3;
		case MxEngine::TextureFormat::RGBA:
			return 4;
		case MxEngine::TextureFormat::RGB16:
			return 6;
		case MxEngine::TextureFormat::RGB16F:
			return 6;
		case MxEngine::TextureFormat::RGBA16:
			return 8;
		case MxEngine::TextureFormat::RGBA16F:
			return 8;
		case MxEngine::TextureFormat::RGB32F:
			return 12;
		case MxEngine::TextureFormat::RGBA32F:
			return 16;
		case MxEngine::TextureFormat::DEPTH:
			return 1;
		case MxEngine::TextureFormat::DEPTH32F:
			return 4;
		default:
			return 0;
		}
	}

	TextureFormat Texture::GetFormat() const
	{
		return this->format;
	}

	TextureWrap Texture::GetWrapType() const
	{
		return this->wrapType;
	}

	void Texture::Bind() const
	{
		GLCALL(glActiveTexture(GL_TEXTURE0 + this->activeId));
		GLCALL(glBindTexture(this->textureType, id));
	}

	void Texture::Unbind() const
	{
		GLCALL(glActiveTexture(GL_TEXTURE0 + this->activeId));
		GLCALL(glBindTexture(this->textureType, 0));
	}

	Texture::BindableId Texture::GetBoundId() const
	{
		return this->activeId;
	}

	Texture::BindableId Texture::GetNativeHandle() const
    {
		return id;
    }

	void Texture::Bind(TextureBindId id) const
	{
		this->activeId = id;
		this->Bind();
	}

	const MxString& Texture::GetFilePath() const
	{
		return this->filepath;
	}

	void Texture::SetInternalEngineTag(const MxString& tag)
	{
		this->filepath = tag;
	}

    unsigned int Texture::GetTextureType() const
    {
		return this->textureType;
    }

	size_t Texture::GetWidth() const
	{
		return width;
	}

	size_t Texture::GetHeight() const
	{
		return height;
	}

	size_t Texture::GetChannelCount() const
	{
		switch (this->format)
		{
		case MxEngine::TextureFormat::R:
			return 1;
		case MxEngine::TextureFormat::R16:
			return 1;
		case MxEngine::TextureFormat::RG:
			return 2;
		case MxEngine::TextureFormat::RG16:
			return 2;
		case MxEngine::TextureFormat::R16F:
			return 1;
		case MxEngine::TextureFormat::R32F:
			return 1;
		case MxEngine::TextureFormat::RG16F:
			return 2;
		case MxEngine::TextureFormat::RG32F:
			return 2;
		case MxEngine::TextureFormat::RGB:
			return 3;
		case MxEngine::TextureFormat::RGBA:
			return 4;
		case MxEngine::TextureFormat::RGB16:
			return 3;
		case MxEngine::TextureFormat::RGB16F:
			return 3;
		case MxEngine::TextureFormat::RGBA16:
			return 4;
		case MxEngine::TextureFormat::RGBA16F:
			return 4;
		case MxEngine::TextureFormat::RGB32F:
			return 3;
		case MxEngine::TextureFormat::RGBA32F:
			return 4;
		case MxEngine::TextureFormat::DEPTH:
			return 1;
		case MxEngine::TextureFormat::DEPTH32F:
			return 1;
		default:
			return 0;
		}
	}

    const char* EnumToString(TextureFormat format)
    {
		#define TEX_FMT_STR(val) case TextureFormat::val: return #val
		switch (format)
		{
			TEX_FMT_STR(R);
			TEX_FMT_STR(R16);
			TEX_FMT_STR(RG);
			TEX_FMT_STR(RG16);
			TEX_FMT_STR(R16F);
			TEX_FMT_STR(R32F);
			TEX_FMT_STR(RG16F);
			TEX_FMT_STR(RG32F);
			TEX_FMT_STR(RGB);
			TEX_FMT_STR(RGBA);
			TEX_FMT_STR(RGB16);
			TEX_FMT_STR(RGB16F);
			TEX_FMT_STR(RGBA16);
			TEX_FMT_STR(RGBA16F);
			TEX_FMT_STR(RGB32F);
			TEX_FMT_STR(RGBA32F);
			TEX_FMT_STR(DEPTH);
			TEX_FMT_STR(DEPTH32F);
			default: return "INVALID_FORMAT";
		}
    }

    const char* EnumToString(TextureWrap wrap)
    {
		#define TEX_WRAP_STR(val) case TextureWrap::val: return #val
		switch (wrap)
		{
			TEX_WRAP_STR(CLAMP_TO_EDGE);
			TEX_WRAP_STR(CLAMP_TO_BORDER);
			TEX_WRAP_STR(MIRRORED_REPEAT);
			TEX_WRAP_STR(REPEAT);
		default:
			return "INVALID_WRAPTYPE";
		}
    }
}