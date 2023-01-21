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

#ifndef RMLUI_CORE_LAYOUTINLINELEVELBOXTEXT_H
#define RMLUI_CORE_LAYOUTINLINELEVELBOXTEXT_H

#include "LayoutInlineLevelBox.h"

namespace Rml {

class ElementText;

/**
    Inline-level text boxes represent text nodes.

    Generates fragments to display its text, splitting it up as necessary to fit in the available space.
 */
class InlineLevelBox_Text final : public InlineLevelBox {
public:
	InlineLevelBox_Text(ElementText* element);

	FragmentResult CreateFragment(InlineLayoutMode mode, float available_width, float right_spacing_width, bool first_box,
		LayoutOverflowHandle overflow_handle) override;

	void Submit(FragmentBox fragment_box) override;

	String DebugDumpNameValue() const override;

private:
	ElementText* GetTextElement();

	Vector2f element_offset;
	StringList fragments;
};

String FontFaceDescription(const String& font_family, Style::FontStyle style, Style::FontWeight weight);

} // namespace Rml
#endif
