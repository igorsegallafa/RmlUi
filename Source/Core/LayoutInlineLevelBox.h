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
struct FragmentResult;
struct FragmentBox;

using LayoutOverflowHandle = int;

/**
    A box that takes part in inline layout.

    The inline-level box is used to generate fragments that are placed within line boxes.
 */
class InlineLevelBox {
public:
	virtual ~InlineLevelBox();

	// Create a fragment from this box, if it can fit within the available width.
	virtual FragmentResult CreateFragment(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle) = 0;

	// Submit a fragment's position and size to be displayed on the underlying element.
	virtual void Submit(FragmentBox fragment_box, String text) = 0;

	virtual String DebugDumpNameValue() const = 0;
	virtual String DebugDumpTree(int depth) const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

protected:
	InlineLevelBox(Element* element) : element(element) {}

	Element* GetElement() const { return element; }

	void SubmitBox(Box box, const FragmentBox& fragment_box);

	void OnLayout() { element->OnLayout(); } // TODO

private:
	Element* element;
};

/**
    Atomic inline-level boxes are sized boxes that cannot be split.

    This includes inline-block elements, replaced inline-level elements, inline tables, and inline flex containers.
 */
class InlineLevelBox_Atomic final : public InlineLevelBox {
public:
	InlineLevelBox_Atomic(Element* element, const Box& box);

	FragmentResult CreateFragment(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle) override;
	void Submit(FragmentBox fragment_box, String text) override;

	String DebugDumpNameValue() const override { return "InlineLevelBox_Atomic"; }

private:
	Box box;
};

enum class FragmentType {
	Invalid,    // Could not be placed.
	InlineBox,  // An inline box.
	Principal,  // The element's first and main fragment for inline-level boxes that are not inline-boxes.
	Additional, // Positioned relative to the element's principal fragment.
};

struct FragmentResult {
	FragmentResult() = default;
	FragmentResult(FragmentType type, Vector2f layout_bounds, float spacing_left = 0.f, float spacing_right = 0.f,
		LayoutOverflowHandle overflow_handle = {}, String text = {}) :
		type(type),
		layout_bounds(layout_bounds), spacing_left(spacing_left), spacing_right(spacing_right), overflow_handle(overflow_handle),
		text(std::move(text))
	{}

	FragmentType type = FragmentType::Invalid;
	Vector2f layout_bounds;

	float spacing_left = 0.f;  // Left margin-border-padding for inline boxes.
	float spacing_right = 0.f; // Right margin-border-padding for inline boxes.

	// Overflow handle is non-zero when there is another fragment to be layed out.
	LayoutOverflowHandle overflow_handle = {};

	String text;
};

struct FragmentBox {
	Element* offset_parent;
	Vector2f position;
	Vector2f size;
	bool principal_box;
	bool split_left;
	bool split_right;
};

} // namespace Rml
#endif
