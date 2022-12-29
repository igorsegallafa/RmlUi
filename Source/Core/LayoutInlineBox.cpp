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

static float GetEdgeSize(const Box& box, Box::Edge edge)
{
	return box.GetEdge(Box::PADDING, edge) + box.GetEdge(Box::BORDER, edge) + box.GetEdge(Box::MARGIN, edge);
}

String LayoutElementName(Element* element)
{
	if (!element)
		return "nullptr";
	if (!element->GetId().empty())
		return '#' + element->GetId();
	if (auto element_text = rmlui_dynamic_cast<ElementText*>(element))
		return '\"' + element_text->GetText().substr(0, 20) + '\"';
	return element->GetAddress();
}

void* InlineLevelBox::operator new(size_t size)
{
	return LayoutEngine::AllocateLayoutChunk(size);
}

void InlineLevelBox::operator delete(void* chunk, size_t size)
{
	LayoutEngine::DeallocateLayoutChunk(chunk, size);
}

InlineLevelBox::~InlineLevelBox() {}

float InlineLevelBox::GetEdge(Box::Edge /*edge*/) const
{
	return 0.f;
}

String InlineLevelBox::DebugDumpTree(int depth) const
{
	String value = String(depth * 2, ' ') + DebugDumpNameValue() + " | " + LayoutElementName(GetElement()) + '\n';
	return value;
}

String InlineBoxBase::DebugDumpTree(int depth) const
{
	String value = InlineLevelBox::DebugDumpTree(depth);
	for (auto&& child : children)
		value += child->DebugDumpTree(depth + 1);
	return value;
}

LayoutFragment InlineBoxRoot::LayoutContent(bool /*first_box*/, float /*available_width*/, float /*right_spacing_width*/)
{
	return {};
}

LayoutFragment InlineBox::LayoutContent(bool first_box, float available_width, float right_spacing_width)
{
	if (first_box || right_spacing_width <= available_width)
		return LayoutFragment{this, Vector2f(-1.f, GetElement()->GetLineHeight())};

	return {};
}

float InlineBox::GetEdge(Box::Edge edge) const
{
	return GetEdgeSize(box, edge);
}

LayoutFragment InlineLevelBox_Atomic::LayoutContent(bool first_box, float available_width, float right_spacing_width)
{
	const Vector2f outer_size = {
		box.GetSizeAcross(Box::HORIZONTAL, Box::MARGIN),
		box.GetSizeAcross(Box::VERTICAL, Box::MARGIN),
	};

	if (first_box || outer_size.x + right_spacing_width <= available_width)
		return LayoutFragment{this, outer_size};

	return {};
}

} // namespace Rml
