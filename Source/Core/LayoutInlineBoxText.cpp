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

#include "LayoutInlineBoxText.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/FontEngineInterface.h"
#include "../../Include/RmlUi/Core/Log.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "ComputeProperty.h"
#include "LayoutEngine.h"
#include "LayoutLineBox.h"

namespace Rml {

// TODO: Don't need it here anymore? Move it.
String FontFaceDescription(const String& font_family, Style::FontStyle style, Style::FontWeight weight)
{
	String font_attributes;

	if (style == Style::FontStyle::Italic)
		font_attributes += "italic, ";
	if (weight == Style::FontWeight::Bold)
		font_attributes += "bold, ";
	else if (weight != Style::FontWeight::Auto && weight != Style::FontWeight::Normal)
		font_attributes += "weight=" + ToString((int)weight) + ", ";

	if (font_attributes.empty())
		font_attributes = "regular";
	else
		font_attributes.resize(font_attributes.size() - 2);

	return CreateString(font_attributes.size() + font_family.size() + 8, "'%s' [%s]", font_family.c_str(), font_attributes.c_str());
}

LayoutFragment InlineLevelBox_Text::LayoutContent(bool first_box, float available_width, float right_spacing_width)
{
	ElementText* text_element = GetTextElement();

	int line_begin = 0; // TODO: Handle split fragment
	int line_length = 0;
	float line_width = 0.f;
	bool overflow =
		!text_element->GenerateLine(line_contents, line_length, line_width, line_begin, available_width, right_spacing_width, first_box, true);

	LayoutOverflowHandle overflow_handle = {};
	if (overflow)
		overflow_handle = line_length;

	const Vector2f fragment_size = {line_width, text_element->GetLineHeight()};

	return LayoutFragment(this, fragment_size, overflow_handle);
}

String InlineLevelBox_Text::DebugDumpNameValue() const
{
	return "InlineLevelBox_Text";
}

ElementText* InlineLevelBox_Text::GetTextElement()
{
	RMLUI_ASSERT(rmlui_dynamic_cast<ElementText*>(GetElement()));

	return static_cast<ElementText*>(GetElement());
}

} // namespace Rml
