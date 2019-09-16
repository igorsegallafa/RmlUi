/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "precompiled.h"
#include <RmlUi/Core.h>
#include "FontFamily.h"
#include "FreeType/FontProvider.h"
#include "BitmapFont/FontProvider.h"
#include "FontDatabaseDefault.h"

namespace Rml {
namespace Core {

#ifndef RMLUI_NO_FONT_INTERFACE_DEFAULT

FontDatabaseDefault* FontDatabaseDefault::instance = nullptr;
FontDatabaseDefault::FontProviderTable FontDatabaseDefault::font_provider_table;

FontDatabaseDefault::FontDatabaseDefault()
{
	RMLUI_ASSERT(instance == nullptr);
	instance = this;
}

FontDatabaseDefault::~FontDatabaseDefault()
{
	RMLUI_ASSERT(instance == this);
	instance = nullptr;
}

bool FontDatabaseDefault::Initialise()
{
	if (instance == nullptr)
	{
		new FontDatabaseDefault();

        if(!FreeType::FontProvider::Initialise())
            return false;

        if(!BitmapFont::FontProvider::Initialise())
            return false;
	}

	return true;
}

void FontDatabaseDefault::Shutdown()
{
	if (instance != nullptr)
	{
        FreeType::FontProvider::Shutdown();
        BitmapFont::FontProvider::Shutdown();

		delete instance;
	}
}

// Loads a new font face.
bool FontDatabaseDefault::LoadFontFace(const String& file_name)
{
    FontProviderType font_provider_type = GetFontProviderType(file_name);

    switch(font_provider_type)
    {
        case FreeType:
            return FreeType::FontProvider::LoadFontFace(file_name);

        case BitmapFont:
            return BitmapFont::FontProvider::LoadFontFace(file_name);

        default:
            return false;
    }
}

FontDatabaseDefault::FontProviderType FontDatabaseDefault::GetFontProviderType(const String& file_name)
{
    if(file_name.find(".fnt") != String::npos)
    {
        return BitmapFont;
    }
    else
    {
        return FreeType;
    }
}

// Returns a handle to a font face that can be used to position and render text.
SharedPtr<FontFaceHandleDefault> FontDatabaseDefault::GetFontFaceHandle(const String& family, const String& charset, Style::FontStyle style, Style::FontWeight weight, int size)
{
    size_t provider_count = font_provider_table.size();

    for(size_t provider_index = 0; provider_index < provider_count; ++provider_index)
    {
		SharedPtr<FontFaceHandleDefault> face_handle = font_provider_table[ provider_index ]->GetFontFaceHandle(family, charset, style, weight, size);

        if(face_handle)
            return face_handle;
    }

    return nullptr;
}

void FontDatabaseDefault::AddFontProvider(FontProvider * provider)
{
    instance->font_provider_table.push_back(provider);
}

void FontDatabaseDefault::RemoveFontProvider(FontProvider * provider)
{
    for(FontProviderTable::iterator i = instance->font_provider_table.begin(); i != instance->font_provider_table.end(); ++i)
    {
        if(*i == provider)
        {
            instance->font_provider_table.erase(i);
            return;
        }
    }
}

#endif

}
}
