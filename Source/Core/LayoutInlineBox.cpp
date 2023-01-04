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
	return element->GetAddress(false, false);
}

void* InlineLevelBox::operator new(size_t size)
{
	return LayoutEngine::AllocateLayoutChunk(size);
}

void InlineLevelBox::operator delete(void* chunk, size_t size)
{
	LayoutEngine::DeallocateLayoutChunk(chunk, size);
}

void InlineLevelBox::SubmitBox(Box box, const BoxDisplay& box_display)
{
	RMLUI_ASSERT(element && element != box_display.offset_parent);

	box.SetContent(box_display.size);

	if (box_display.split_left)
		ZeroBoxEdge(box, Box::LEFT);
	if (box_display.split_right)
		ZeroBoxEdge(box, Box::RIGHT);

	Vector2f offset = box_display.position;
	offset.x += box.GetEdge(Box::MARGIN, Box::LEFT);

	if (box_display.principal_box)
	{
		element->SetOffset(offset, box_display.offset_parent);
		element->SetBox(box);
		OnLayout();
	}
	else
	{
		// TODO: Will be wrong in case of relative positioning. (we really just want to subtract the value submitted to SetOffset in Submit()
		// above).
		const Vector2f element_offset = element->GetRelativeOffset(Box::BORDER);
		element->AddBox(box, offset - element_offset);
	}
}

InlineLevelBox::~InlineLevelBox() {}

String InlineLevelBox::DebugDumpTree(int depth) const
{
	String value = String(depth * 2, ' ') + DebugDumpNameValue() + " | " + LayoutElementName(GetElement()) + '\n';
	return value;
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

LayoutFragment InlineLevelBox_Atomic::LayoutContent(bool first_box, float available_width, float right_spacing_width,
	LayoutOverflowHandle overflow_handle)
{
	const Vector2f outer_size = {
		box.GetSizeAcross(Box::HORIZONTAL, Box::MARGIN),
		box.GetSizeAcross(Box::VERTICAL, Box::MARGIN),
	};

	if (first_box || outer_size.x + right_spacing_width <= available_width)
		return LayoutFragment(FragmentType::Principal, outer_size, 0.f, 0.f);

	return {};
}

void InlineLevelBox_Atomic::Submit(BoxDisplay box_display, String /*text*/)
{
	box_display.size = box.GetSize();
	SubmitBox(box, box_display);
}

} // namespace Rml
