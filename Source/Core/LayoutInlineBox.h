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

#ifndef RMLUI_CORE_LAYOUTINLINEBOX_H
#define RMLUI_CORE_LAYOUTINLINEBOX_H

#include "../../Include/RmlUi/Core/Box.h"
#include "LayoutInlineLevelBox.h"

namespace Rml {

class InlineBoxBase : public InlineLevelBox {
public:
	InlineLevelBox* AddChild(UniquePtr<InlineLevelBox> child);

	String DebugDumpTree(int depth) const override;

protected:
	InlineBoxBase(Element* element) : InlineLevelBox(element) {}

private:
	using InlineBoxList = Vector<UniquePtr<InlineLevelBox>>;

	// @performance Use first_child, next_sibling instead to build the tree?
	InlineBoxList children;
};

class InlineBoxRoot final : public InlineBoxBase {
public:
	InlineBoxRoot() : InlineBoxBase(nullptr) {}

	LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle) override;
	void Submit(BoxDisplay box_display, String text) override;

	String DebugDumpNameValue() const override { return "InlineBoxRoot"; }
};

class InlineBox final : public InlineBoxBase {
public:
	InlineBox(Element* element, const Box& box) : InlineBoxBase(element), box(box) { RMLUI_ASSERT(element && box.GetSize().x < 0.f); }

	LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle) override;
	void Submit(BoxDisplay box_display, String text) override;

	String DebugDumpNameValue() const override { return "InlineBox"; }

private:
	Box box;
};

} // namespace Rml
#endif
