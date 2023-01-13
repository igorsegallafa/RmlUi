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

#ifndef RMLUI_CORE_LAYOUTLINEBOX_H
#define RMLUI_CORE_LAYOUTLINEBOX_H

#include "LayoutInlineLevelBox.h"

namespace Rml {

class InlineBoxRoot;

class LayoutLineBox final {
public:
	LayoutLineBox() = default;
	~LayoutLineBox();

	// Returns true if the box should be placed again on a new line.
	bool AddBox(InlineLevelBox* box, InlineLayoutMode layout_mode, float line_width, LayoutOverflowHandle& inout_overflow_handle);

	// Closes the line, submitting any fragments placed on this line.
	// @param[out] out_height_of_line Height of line. Note: This can be different from the element's computed line-height property.
	// @return The next line, containing any open fragments that had to be split or wrapped down.
	UniquePtr<LayoutLineBox> Close(const InlineBoxRoot* root_box, Element* offset_parent, Vector2f line_position, Style::TextAlign text_align,
		float& out_height_of_line);

	/// Close the open inline box.
	/// @param[in] inline_box The inline box to be closed. Should match the currently open box, strictly used for verification.
	/// @note Only inline-boxes need to be closed. Other inline-level boxes do not contain child boxes considered in the current inline formatting
	void CloseInlineBox(InlineBox* inline_box);

	InlineBox* GetOpenInlineBox();

	float GetBoxCursor() const { return box_cursor; }
	bool IsClosed() const { return is_closed; }
	bool HasContent() const { return fragments.size() > open_fragments.size(); }

	String DebugDumpTree(int depth) const;

	void SetLineBox(Vector2f line_position, float line_width);
	Vector2f GetPosition() const { return line_position; }

	// Returns the width of the contents of the line, relative to the line position. Includes spacing due to horizontal alignment.
	// @note Only available after line has been closed.
	float GetExtentRight() const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

private:
	using FragmentIndex = int;

	struct Fragment {
		enum class Type { SizedBox, InlineBox };

		Type type = Type::SizedBox;

		InlineLevelBox* inline_level_box;

		Vector2f position;      // Outer (top-left) position relative to start of the line, disregarding floats.
		Vector2f layout_bounds; // Outer size for replaced and inline blocks, inner size for inline boxes.

		String text; // @performance Replace by a pointer or index? Don't need it for most fragments.

		float total_height_above_baseline = 0.f;
		float total_depth_below_baseline = 0.f;

		bool principal_fragment = true;
		bool split_left = false;
		bool split_right = false;

		// For inline boxes
		bool has_content = false;
		float spacing_left = 0.f;             // Left margin-border-padding for inline boxes.
		float spacing_right = 0.f;            // Right margin-border-padding for inline boxes.
		FragmentIndex children_end_index = 0; // One-past-last-child of this box, as index into fragment list.

		float baseline_offset = 0.f; // Vertical offset from root baseline to our baseline.
	};

	using FragmentList = Vector<Fragment>;
	using FragmentIndexList = Vector<FragmentIndex>;

	// Place an open fragment.
	Fragment& CloseFragment(int open_fragment_index, float inner_right_position);

	// Splits the line, returning a new line if there are any open fragments.
	UniquePtr<LayoutLineBox> SplitLine();

	// Position of the line, relative to our parent root.
	Vector2f line_position;
	// Available space for the line. Based on our parent box content width, possibly shrinked due to floating boxes.
	float line_width = 0.f;

	// Content offset due to space distribution from 'text-align'.
	float offset_horizontal_alignment = 0.f;

	// The horizontal cursor. This is the outer-right position of the last placed fragment.
	float box_cursor = 0.f;
	// The contribution of opened inline boxes to the placement of the next fragment, due to their left edges (margin-border-padding).
	float open_spacing_left = 0.f;

	// List of placed fragments in this line box.
	FragmentList fragments;

	// List of fragments from inline boxes that have been opened but are yet to be closed, as indices into 'fragments'.
	// @performance Store using parent pointers to avoid allocations.
	FragmentIndexList open_fragments;

	bool is_closed = false;
};

} // namespace Rml
#endif
