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

	// Principal box
	virtual void Submit(BoxDisplay box_display, String /*text*/) { SubmitBox(Box{}, box_display); }

	virtual float GetOuterSpacing(Box::Edge edge) const;

	virtual String DebugDumpNameValue() const = 0;
	virtual String DebugDumpTree(int depth) const;

	void* operator new(size_t size);
	void operator delete(void* chunk, size_t size);

protected:
	InlineLevelBox(Element* element) : element(element) {}

	Element* GetElement() const { return element; }

	void SubmitBox(Box box, const BoxDisplay& box_display)
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

class InlineBoxBase : public InlineLevelBox {
public:
	InlineLevelBox* AddChild(UniquePtr<InlineLevelBox> child)
	{
		auto result = child.get();
		children.push_back(std::move(child));
		return result;
	}

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
	String DebugDumpNameValue() const override { return "InlineBoxRoot"; }
};

class InlineBox final : public InlineBoxBase {
public:
	InlineBox(Element* element, const Box& box) : InlineBoxBase(element), box(box) { RMLUI_ASSERT(element && box.GetSize().x < 0.f); }

	LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle) override;
	float GetOuterSpacing(Box::Edge edge) const override;

	void Submit(BoxDisplay box_display, String /*text*/) override { SubmitBox(box, box_display); }

	String DebugDumpNameValue() const override { return "InlineBox"; }

private:
	Box box;
};

class InlineLevelBox_Atomic final : public InlineLevelBox {
public:
	InlineLevelBox_Atomic(Element* element, const Box& box) : InlineLevelBox(element), box(box)
	{
		RMLUI_ASSERT(element);
		RMLUI_ASSERT(box.GetSize().x >= 0.f && box.GetSize().y >= 0.f);
	}

	LayoutFragment LayoutContent(bool first_box, float available_width, float right_spacing_width, LayoutOverflowHandle overflow_handle) override;

	String DebugDumpNameValue() const override { return "InlineLevelBox_Atomic"; }

	void Submit(BoxDisplay box_display, String /*text*/) override
	{
		box_display.size = box.GetSize();
		SubmitBox(box, box_display);
	}

private:
	Box box;
};

struct LayoutFragment {
	enum class Type {
		Invalid,
		Principal, // The element's first and main fragment.
		Secondary, // Positioned relative to the element's principal fragment.
	};
	enum class Split { Closed, OpenLeft, OpenRight, OpenBoth };

	LayoutFragment() = default;
	LayoutFragment(Vector2f layout_bounds, Type type = Type::Principal, Split split = Split::Closed, LayoutOverflowHandle overflow_handle = {},
		String text = {}) :
		layout_bounds(layout_bounds),
		type(type), split(split), overflow_handle(overflow_handle), text(std::move(text))
	{}

	explicit operator bool() const { return type != Type::Invalid; }

	Vector2f layout_bounds;

	Type type = Type::Invalid;
	Split split = Split::Closed;

	// Overflow handle is non-zero when there is another fragment to be layed out.
	// TODO: I think we can make this part of the return value for LayoutContent instead? No need to keep this around. Maybe need a pointer to the
	// next fragment in the chain.
	LayoutOverflowHandle overflow_handle = {};

	String text;
};

String LayoutElementName(Element* element);

} // namespace Rml
#endif
