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

#ifndef RMLUI_CORE_LAYOUTFORMATTINGCONTEXT_H
#define RMLUI_CORE_LAYOUTFORMATTINGCONTEXT_H

#include "../../Include/RmlUi/Core/Types.h"
#include "LayoutBlockBox.h"

namespace Rml {

class Box;

struct FormatSettings {
	const Box* override_initial_box = nullptr;
	Vector2f* out_visible_overflow_size = nullptr;
};

class FormattingContext {
public:
	enum class Type {
		Block,
		Inline,
		Table,
		Flex,
	};
	enum class SizingMode {
		StretchFit,
		MinContent,
		MaxContent,
	};

	// TODO Do we care about parent context?
	static UniquePtr<FormattingContext> ConditionallyCreateIndependentFormattingContext(ContainerBox* parent_container, Element* element);

	// TODO: Consider working directly with the final types instead of using a virtual destructor.
	virtual ~FormattingContext() = default;

	// TODO: Instead of (output) format settings, use function calls (virtual if necessary) as needed.
	virtual void Format(FormatSettings format_settings) = 0;

	virtual UniquePtr<LayoutBox> ExtractRootBox() { return nullptr; }

protected:
	FormattingContext(Type type, ContainerBox* parent_box, Element* root_element) : type(type), parent_box(parent_box), root_element(root_element) {}

	Element* GetRootElement() const { return root_element; }

	ContainerBox* GetParentBoxOfContext() const { return parent_box; }

	static void SubmitElementLayout(Element* element) { element->OnLayout(); }

private:
	Type type;
	ContainerBox* parent_box;
	Element* root_element;
};

class BlockFormattingContext final : public FormattingContext {
public:
	BlockFormattingContext(ContainerBox* parent_box, Element* element);
	~BlockFormattingContext();

	void Format(FormatSettings format_settings) override;

	float GetShrinkToFitWidth() const;

	UniquePtr<LayoutBox> ExtractRootBox() override { return std::move(root_block_container); }

private:
	bool FormatBlockBox(BlockContainer* parent_container, Element* element);

	bool FormatBlockContainerChild(BlockContainer* parent_container, Element* element);
	bool FormatInlineBox(BlockContainer* parent_container, Element* element);

	UniquePtr<LayoutBlockBoxSpace> float_space;
	UniquePtr<BlockContainer> root_block_container;
};

} // namespace Rml
#endif
