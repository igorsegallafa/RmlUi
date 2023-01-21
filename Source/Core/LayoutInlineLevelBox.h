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
struct FontMetrics;

enum class InlineLayoutMode {
	WrapAny,          // Allow wrapping to avoid overflow, even if nothing is placed.
	WrapAfterContent, // Allow wrapping to avoid overflow, but first place at least *some* content on this line.
	Nowrap,           // Place all content on this line, regardless of overflow.
};
using LayoutOverflowHandle = int;
using LayoutFragmentHandle = int;

/**
    A box that takes part in inline layout.

    The inline-level box is used to generate fragments that are placed within line boxes.
 */
class InlineLevelBox {
public:
	virtual ~InlineLevelBox();

	// Create a fragment from this box, if it can fit within the available width.
	virtual FragmentResult CreateFragment(InlineLayoutMode mode, float available_width, float right_spacing_width, bool first_box,
		LayoutOverflowHandle overflow_handle) = 0;

	// Submit a fragment's position and size to be displayed on the underlying element.
	virtual void Submit(FragmentBox fragment_box) = 0;

	float GetHeightAboveBaseline() const { return height_above_baseline; }
	float GetDepthBelowBaseline() const { return depth_below_baseline; }
	float GetVerticalOffsetFromParent() const { return vertical_offset_from_parent; }
	Style::VerticalAlign GetVerticalAlign() const { return vertical_align; }
	float GetSpacingLeft() const { return spacing_left; }
	float GetSpacingRight() const { return spacing_right; }

	virtual String DebugDumpNameValue() const = 0;
	virtual String DebugDumpTree(int depth) const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

protected:
	InlineLevelBox(Element* element) : element(element) { RMLUI_ASSERT(element); }

	Element* GetElement() const { return element; }
	const FontMetrics& GetFontMetrics() const;

	// Set the height used for inline layout, and the vertical offset relative to our parent box.
	void SetHeightAndVerticalAlignment(float height_above_baseline, float depth_below_baseline, const InlineLevelBox* parent);

	// Set the inner-to-outer spacing (margin + border + padding) for inline boxes.
	void SetInlineBoxSpacing(float spacing_left, float spacing_right);

	// Calls Element::OnLayout (proxy for private access to Element).
	void SubmitElementOnLayout();

private:
	float height_above_baseline = 0.f;
	float depth_below_baseline = 0.f;

	Style::VerticalAlign vertical_align;
	float vertical_offset_from_parent = 0.f;

	float spacing_left = 0.f;  // Left margin-border-padding for inline boxes.
	float spacing_right = 0.f; // Right margin-border-padding for inline boxes.

	Element* element;
};

/**
    Atomic inline-level boxes are sized boxes that cannot be split.

    This includes inline-block elements, replaced inline-level elements, inline tables, and inline flex containers.
 */
class InlineLevelBox_Atomic final : public InlineLevelBox {
public:
	InlineLevelBox_Atomic(const InlineLevelBox* parent, Element* element, const Box& box);

	FragmentResult CreateFragment(InlineLayoutMode mode, float available_width, float right_spacing_width, bool first_box,
		LayoutOverflowHandle overflow_handle) override;
	void Submit(FragmentBox fragment_box) override;

	String DebugDumpNameValue() const override { return "InlineLevelBox_Atomic"; }

private:
	float outer_width = 0;
	Box box;
};

enum class FragmentType {
	Invalid,   // Could not be placed.
	InlineBox, // An inline box.
	SizedBox,  // Sized inline-level boxes that are not inline-boxes.
	TextRun,   // Text runs.
};

struct FragmentResult {
	FragmentResult() = default;
	FragmentResult(FragmentType type, float layout_width, LayoutFragmentHandle fragment_handle = {}, LayoutOverflowHandle overflow_handle = {}) :
		type(type), layout_width(layout_width), fragment_handle(fragment_handle), overflow_handle(overflow_handle)
	{}

	FragmentType type = FragmentType::Invalid;
	float layout_width = 0.f;

	LayoutFragmentHandle fragment_handle = {}; // Handle to enable the inline-level box to reference any fragment-specific data.
	LayoutOverflowHandle overflow_handle = {}; // Overflow handle is non-zero when there is another fragment to be layed out.
};

struct FragmentBox {
	Element* offset_parent;
	LayoutFragmentHandle handle;
	Vector2f position;
	float layout_width;
	bool split_left;
	bool split_right;
};

} // namespace Rml
#endif
