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

#include "LayoutInlineBox.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/FontEngineInterface.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "LayoutBlockBox.h"
#include "LayoutEngine.h"

namespace Rml {

String LayoutElementName(Element* element)
{
	if (!element)
		return "nullptr";
	if (!element->GetId().empty())
		return '#' + element->GetId();
	if (element->GetTagName() == "#text")
		return "text";
	return element->GetAddress();
}

LayoutInlineBox::~LayoutInlineBox() {}

String LayoutInlineBox::DebugDumpTree(int depth) const
{
	String value = String(depth * 2, ' ') + DebugDumpNameValue() + " | " + LayoutElementName(element) + '\n';

	for (auto&& child : children)
		value += child->DebugDumpTree(depth + 1);

	return value;
}

void* LayoutInlineBox::operator new(size_t size)
{
	return LayoutEngine::AllocateLayoutChunk(size);
}

void LayoutInlineBox::operator delete(void* chunk, size_t size)
{
	LayoutEngine::DeallocateLayoutChunk(chunk, size);
}

LayoutFragment LayoutInlineBoxSized::LayoutContent(bool /*first_box*/, float available_width, float right_spacing_width)
{
	Vector2f outer_size;
	outer_size.x = box.GetSizeAcross(Box::HORIZONTAL, Box::MARGIN);
	outer_size.y = box.GetSize().y; // Vertical edges are ignored for inline elements.

	if (outer_size.x + right_spacing_width < available_width)
		return LayoutFragment{this, outer_size};

	return {};
}

String LayoutInlineBoxSized::DebugDumpNameValue() const
{
	return "LayoutInlineBoxSized";
}

} // namespace Rml
