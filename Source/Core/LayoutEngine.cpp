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

#include "LayoutEngine.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/PropertySpecification.h"
#include "../../Include/RmlUi/Core/StyleSheetSpecification.h"
#include "../../Include/RmlUi/Core/SystemInterface.h"
#include "../../Include/RmlUi/Core/Types.h"
#include "LayoutBlockBoxSpace.h"
#include "LayoutDetails.h"
#include "LayoutFlex.h"
#include "LayoutFormattingContext.h"
#include "LayoutInlineContainer.h"
#include "LayoutTable.h"
#include "Pool.h"
#include <algorithm>
#include <cstddef>
#include <float.h>

namespace Rml {

// TODO: Move to separate file?
template <size_t Size>
struct LayoutChunk {
	alignas(std::max_align_t) byte buffer[Size];
};

static constexpr std::size_t ChunkSizeBig = std::max({sizeof(BlockContainer), sizeof(InlineContainer)});
static constexpr std::size_t ChunkSizeMedium = std::max({sizeof(InlineBox), sizeof(FlexContainer), sizeof(TableWrapper)});
static constexpr std::size_t ChunkSizeSmall =
	std::max({sizeof(InlineLevelBox_Text), sizeof(InlineLevelBox_Atomic), sizeof(LayoutLineBox), sizeof(LayoutBlockBoxSpace)});

static Pool<LayoutChunk<ChunkSizeBig>> layout_chunk_pool_big(50, true);
static Pool<LayoutChunk<ChunkSizeMedium>> layout_chunk_pool_medium(50, true);
static Pool<LayoutChunk<ChunkSizeSmall>> layout_chunk_pool_small(50, true);

void* LayoutEngine::AllocateLayoutChunk(size_t size)
{
	static_assert(ChunkSizeBig > ChunkSizeMedium && ChunkSizeMedium > ChunkSizeSmall, "The following assumes a strict ordering of the chunk sizes.");

	// Note: If any change is made here, make sure a corresponding change is applied to the deallocation procedure below.
	if (size <= ChunkSizeSmall)
		return layout_chunk_pool_small.AllocateAndConstruct();
	else if (size <= ChunkSizeMedium)
		return layout_chunk_pool_medium.AllocateAndConstruct();
	else if (size <= ChunkSizeBig)
		return layout_chunk_pool_big.AllocateAndConstruct();

	RMLUI_ERROR;
	return nullptr;
}

void LayoutEngine::DeallocateLayoutChunk(void* chunk, size_t size)
{
	// Note: If any change is made here, make sure a corresponding change is applied to the allocation procedure above.
	if (size <= ChunkSizeSmall)
		layout_chunk_pool_small.DestroyAndDeallocate((LayoutChunk<ChunkSizeSmall>*)chunk);
	else if (size <= ChunkSizeMedium)
		layout_chunk_pool_medium.DestroyAndDeallocate((LayoutChunk<ChunkSizeMedium>*)chunk);
	else if (size <= ChunkSizeBig)
		layout_chunk_pool_big.DestroyAndDeallocate((LayoutChunk<ChunkSizeBig>*)chunk);
	else
	{
		RMLUI_ERROR;
	}
}

void LayoutEngine::FormatElement(Element* element, Vector2f containing_block)
{
	auto formatting_context = FormattingContext::ConditionallyCreateIndependentFormattingContext(nullptr, nullptr, element);

	if (!formatting_context)
	{
		Log::Message(Log::LT_ERROR, "Element does not create an independent formatting context and cannot be formatted: %s",
			element->GetAddress().c_str());
		RMLUI_ERROR;
		return;
	}

	// TODO: Can we get rid of the containing block from the below call? Instead, we could make a root containing block
	// here as a separate layout box. 
	// All other calls to this function in principle use LayoutDetails::GetContainingBlock, so it would be nice if we
	// could move that to inside the function.
	formatting_context->Format(containing_block, {});
}

} // namespace Rml
