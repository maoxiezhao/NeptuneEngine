#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\collections\array.h"
#include "stb\stb_rect_pack.h"

namespace VulkanTest
{
	// Use stb_rect_pack is more efficient, but the simple dichotomy space partition here is faster
	template<typename Node>
	struct ReckPacker
	{
		Node* left = nullptr;
		Node* right = nullptr;
		U32 x = 0, y = 0, w = 0, h = 0;
		bool used = false;

	public:
		ReckPacker(U32 x_, U32 y_, U32 width, U32 height) :
			x(x_),
			y(y_),
			w(width),
			h(height)
		{
		}

		~ReckPacker()
		{
			CJING_SAFE_DELETE(left);
			CJING_SAFE_DELETE(right);
		}

		template<class... Args>
		Node* Insert(U32 itemWidth, U32 itemHeight, U32 itemPadding, Args&&...args)
		{
			const U32 paddedWidth = itemWidth + itemPadding;
			const U32 paddedHeight = itemHeight + itemPadding;

			Node* ret = nullptr;
			if (used == false && w == paddedWidth && h == paddedHeight)
			{
				used = true;
				ret = (Node*)this;
				return ret;
			}
			
			// Check children there are empty empty region
			if (left || right)
			{
				if (left)
				{
					ret = left->Insert(itemWidth, itemHeight, itemPadding, std::forward<Args>(args)...);
					if (ret)
						return ret;
				}
				if (right)
				{
					ret = right->Insert(itemWidth, itemHeight, itemPadding, std::forward<Args>(args)...);
					if (ret)
						return ret;
				}
			}

			if (used || paddedWidth > w || paddedHeight > h)
				return nullptr;

			// Split the remainning area by the smaller dimension
			const U32 remainningWidth = std::max(0u, w - paddedWidth);
			const U32 remainningHeight = std::max(0u, h - paddedHeight);

			// EX.
			// -------------------------------------------
			// |	PaddedSpace      	     |    Left	 |
			// |						     |	    	 |
			// ------------------------------------------- -
			// |										 |
			// |										 |
			// |         Right                           | RemainningHeight
			// |										 |
			// ------------------------------------------- -
			//                               |           |
			//							   RemainningWeidth

			if (remainningWidth <= remainningHeight)
			{
				// Split horizontally
				left = CJING_NEW(Node)(x + paddedWidth, y, remainningWidth, paddedHeight);
				right = CJING_NEW(Node)(x, y + paddedHeight, w, remainningHeight);
			}
			else
			{
				// Split vertically
				left = CJING_NEW(Node)(x, y + paddedHeight, paddedWidth, remainningHeight);
				right = CJING_NEW(Node)(x + paddedWidth, y, remainningWidth, h);
			}

			w = paddedWidth;
			h = paddedHeight;
			used = true;

			return (Node*)this;
		}
	};
}