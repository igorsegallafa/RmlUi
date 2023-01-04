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

#ifndef RMLUI_CORE_LAYOUTINLINELEVELBOX_H
#define RMLUI_CORE_LAYOUTINLINELEVELBOX_H

#include "../../Include/RmlUi/Core/Box.h"
#include "../../Include/RmlUi/Core/StyleTypes.h"

namespace Rml {

class Element;
struct LayoutFragment;

using LayoutOverflowHandle = int;

struct BoxDisplay {
	Element* offset_parent;
	Vector2f position;
	Vector2f size;
	bool principal_box;
	bool split_left;
	bool split_right;
};

class InlineLevelBox {
public:
	virtual ~InlineLevelBox();

	virtual LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle) = 0;

	// Position and size element's box.
	virtual void Submit(BoxDisplay box_display, String /*text*/) = 0;

	virtual String DebugDumpNameValue() const = 0;
	virtual String DebugDumpTree(int depth) const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

protected:
	InlineLevelBox(Element* element) : element(element) {}

	Element* GetElement() const { return element; }

	void SubmitBox(Box box, const BoxDisplay& box_display);

	void OnLayout() { element->OnLayout(); } // TODO

private:
	static void ZeroBoxEdge(Box& box, Box::Edge edge)
	{
		box.SetEdge(Box::PADDING, edge, 0.f);
		box.SetEdge(Box::BORDER, edge, 0.f);
		box.SetEdge(Box::MARGIN, edge, 0.f);
	}

	Element* element;
};

class InlineLevelBox_Atomic final : public InlineLevelBox {
public:
	InlineLevelBox_Atomic(Element* element, const Box& box) : InlineLevelBox(element), box(box)
	{
		RMLUI_ASSERT(element);
		RMLUI_ASSERT(box.GetSize().x >= 0.f && box.GetSize().y >= 0.f);
	}

	LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle) override;
	void Submit(BoxDisplay box_display, String text) override;

	String DebugDumpNameValue() const override { return "InlineLevelBox_Atomic"; }

private:
	Box box;
};

enum class FragmentType {
	Invalid,    // Could not be placed.
	InlineBox,  // An inline-box.
	Principal,  // The element's first and main fragment for inline-level boxes that are not inline-boxes.
	Additional, // Positioned relative to the element's principal fragment.
};

struct LayoutFragment {
	LayoutFragment() = default;
	LayoutFragment(FragmentType type, Vector2f layout_bounds, float spacing_left = 0.f, float spacing_right = 0.f,
		LayoutOverflowHandle overflow_handle = {}, String text = {}) :
		type(type),
		layout_bounds(layout_bounds), spacing_left(spacing_left), spacing_right(spacing_right), overflow_handle(overflow_handle),
		text(std::move(text))
	{}

	FragmentType type = FragmentType::Invalid;

	Vector2f layout_bounds;

	float spacing_left = 0.f;  // Padding-border-margin left
	float spacing_right = 0.f; // Padding-border-margin right

	// Overflow handle is non-zero when there is another fragment to be layed out.
	LayoutOverflowHandle overflow_handle = {};

	String text;
};

} // namespace Rml
#endif
