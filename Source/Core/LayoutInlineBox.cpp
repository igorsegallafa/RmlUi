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

// TODO: Includes

namespace Rml {

static float GetEdgeSize(const Box& box, Box::Edge edge)
{
	return box.GetEdge(Box::PADDING, edge) + box.GetEdge(Box::BORDER, edge) + box.GetEdge(Box::MARGIN, edge);
}

InlineLevelBox* InlineBoxBase::AddChild(UniquePtr<InlineLevelBox> child)
{
	auto result = child.get();
	children.push_back(std::move(child));
	return result;
}

String InlineBoxBase::DebugDumpTree(int depth) const
{
	String value = InlineLevelBox::DebugDumpTree(depth);
	for (auto&& child : children)
		value += child->DebugDumpTree(depth + 1);
	return value;
}

LayoutFragment InlineBoxRoot::LayoutContent(bool /*first_box*/, float /*available_width*/, float /*right_spacing_width*/,
	LayoutOverflowHandle /*overflow_handle*/)
{
	return {};
}

void InlineBoxRoot::Submit(BoxDisplay /*box_display*/, String /*text*/)
{
	RMLUI_ERROR;
}

LayoutFragment InlineBox::LayoutContent(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle)
{
	const float edge_left = GetEdgeSize(box, Box::LEFT);
	const float edge_right = GetEdgeSize(box, Box::RIGHT);
	if (first_box || right_spacing_width <= available_width + edge_left)
		return LayoutFragment(FragmentType::InlineBox, Vector2f(-1.f, GetElement()->GetLineHeight()), edge_left, edge_right);

	return {};
}

void InlineBox::Submit(BoxDisplay box_display, String /*text*/)
{
	SubmitBox(box, box_display);
}

} // namespace Rml
