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

#include "LayoutInlineLevelBox.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/FontEngineInterface.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "LayoutBlockBox.h"
#include "LayoutDetails.h"
#include "LayoutEngine.h"

// TODO: Includes

namespace Rml {

static void ZeroBoxEdge(Box& box, Box::Edge edge)
{
	box.SetEdge(Box::PADDING, edge, 0.f);
	box.SetEdge(Box::BORDER, edge, 0.f);
	box.SetEdge(Box::MARGIN, edge, 0.f);
}

void* InlineLevelBox::operator new(size_t size)
{
	return LayoutEngine::AllocateLayoutChunk(size);
}

void InlineLevelBox::operator delete(void* chunk, size_t size)
{
	LayoutEngine::DeallocateLayoutChunk(chunk, size);
}

void InlineLevelBox::SubmitBox(Box box, const FragmentBox& fragment_box)
{
	RMLUI_ASSERT(element && element != fragment_box.offset_parent);

	box.SetContent(fragment_box.size);

	if (fragment_box.split_left)
		ZeroBoxEdge(box, Box::LEFT);
	if (fragment_box.split_right)
		ZeroBoxEdge(box, Box::RIGHT);

	Vector2f offset = fragment_box.position;
	offset.x += box.GetEdge(Box::MARGIN, Box::LEFT);

	if (fragment_box.principal_box)
	{
		element->SetOffset(offset, fragment_box.offset_parent);
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
	String value = String(depth * 2, ' ') + DebugDumpNameValue() + " | " + LayoutDetails::GetDebugElementName(GetElement()) + '\n';
	return value;
}

InlineLevelBox_Atomic::InlineLevelBox_Atomic(Element* element, const Box& box) : InlineLevelBox(element), box(box)
{
	RMLUI_ASSERT(element);
	RMLUI_ASSERT(box.GetSize().x >= 0.f && box.GetSize().y >= 0.f);
}

FragmentResult InlineLevelBox_Atomic::CreateFragment(bool first_box, float available_width, float right_spacing_width,
	LayoutOverflowHandle /*overflow_handle*/)
{
	const Vector2f outer_size = {
		box.GetSizeAcross(Box::HORIZONTAL, Box::MARGIN),
		box.GetSizeAcross(Box::VERTICAL, Box::MARGIN),
	};

	if (first_box || outer_size.x + right_spacing_width <= available_width)
		return FragmentResult(FragmentType::Principal, outer_size, 0.f, 0.f);

	return {};
}

void InlineLevelBox_Atomic::Submit(FragmentBox fragment_box, String /*text*/)
{
	fragment_box.size = box.GetSize();
	SubmitBox(box, fragment_box);
}

} // namespace Rml
