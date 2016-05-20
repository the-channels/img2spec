int gC64Palette[16]
{
		0x000000,
		0xFFFFFF,
		0x744335,
		0x7CACBA,
		0x7B4890,
		0x64974F,
		0x403285,
		0xBFCD7A,
		0x7B5B2F,
		0x4f4500,
		0xa37265,
		0x505050,
		0x787878,
		0xa4d78e,
		0x786abd,
		0x9f9f9f
};

class C64HiresDevice : public Device
{
public:

	int mOptAttribOrder;
	int mOptBright;
	int mOptPaper;
	int mOptCellSize;
	int mOptScreenOrder;
	int mOptWidthCells;
	int mOptHeightCells;

	// Spectrum format data
	unsigned char mSpectrumAttributes[128 * 64 * 8 * 2]; // big enough for 8x1 attribs in 3x64 mode at 1024x512
	unsigned char mSpectrumBitmap[128 * 512];
	unsigned char mSpectrumBitmapLinear[128 * 512];

	C64HiresDevice()
	{
		mOptAttribOrder = 0;
		mOptBright = 32;
		mOptPaper = 0;
		mOptCellSize = 0;
		mOptScreenOrder = 1;

		mXRes = 320;
		mYRes = 200;

		mOptWidthCells = mXRes / 8;
		mOptHeightCells = mYRes / 8;
	}

	int rgb_to_c64_pal(int c, int first, int count)
	{
		int i;
		int r = (c >> 16) & 0xff;
		int g = (c >> 8) & 0xff;
		int b = (c >> 0) & 0xff;
		int bestdist = 256 * 256 * 4;
		int best = 0;

		for (i = first; i < first + count; i++)
		{
			int s_b = (gC64Palette[i] >> 0) & 0xff;
			int s_g = (gC64Palette[i] >> 8) & 0xff;
			int s_r = (gC64Palette[i] >> 16) & 0xff;

			int dist = (r - s_r) * (r - s_r) +
				(g - s_g) * (g - s_g) +
				(b - s_b) * (b - s_b);

			if (dist < bestdist)
			{
				best = i;
				bestdist = dist;
			}
		}
		return best; // gSpeccyPalette[best] | 0xff000000;
	}

	virtual int estimate_rgb(int c)
	{
		return gC64Palette[rgb_to_c64_pal(c, 0, 16)] | 0xff000000;
	}

	int pick_from_2_c64_cols(int c, int col1, int col2)
	{
		int r = (c >> 16) & 0xff;
		int g = (c >> 8) & 0xff;
		int b = (c >> 0) & 0xff;

		int s_b = (gC64Palette[col1] >> 0) & 0xff;
		int s_g = (gC64Palette[col1] >> 8) & 0xff;
		int s_r = (gC64Palette[col1] >> 16) & 0xff;

		int dist1 = (r - s_r) * (r - s_r) +
			(g - s_g) * (g - s_g) +
			(b - s_b) * (b - s_b);

		s_b = (gC64Palette[col2] >> 0) & 0xff;
		s_g = (gC64Palette[col2] >> 8) & 0xff;
		s_r = (gC64Palette[col2] >> 16) & 0xff;

		int dist2 = (r - s_r) * (r - s_r) +
			(g - s_g) * (g - s_g) +
			(b - s_b) * (b - s_b);

		if (dist1 < dist2)
			return col1;
		return col2;
	}

	virtual void filter()
	{
		int x, y, i, j;

		// Find closest colors in the speccy palette
		for (i = 0; i < gDevice->mXRes * gDevice->mYRes; i++)
		{
			gBitmapSpec[i] = rgb_to_c64_pal(gBitmapProc[i], 0, 16);
		}

		int ymax;
		int cellht;
		switch (mOptCellSize)
		{
			//case 0:
		default:
			cellht = 8;
			break;
		case 1:
			cellht = 4;
			break;
		case 2:
			cellht = 2;
			break;
		case 3:
			cellht = 1;
			break;
		}
		ymax = gDevice->mYRes / cellht;

		for (y = 0; y < ymax; y++)
		{
			for (x = 0; x < (gDevice->mXRes / 8); x++)
			{				
				int counts[16];
				for (i = 0; i < 16; i++)
					counts[i] = 0;


				for (i = 0; i < cellht; i++)
				{
					for (j = 0; j < 8; j++)
					{
						int loc = (y * cellht + i) * gDevice->mXRes + x * 8 + j;
						int r = rgb_to_c64_pal(gBitmapProc[loc], 0, 16);
						gBitmapSpec[loc] = r;
						counts[r]++;
					}
				}

				// Find two most common colors in cell
				int col1 = 0, col2 = 0;
				int best = 0;
				if (mOptPaper == 0)
				{
					for (i = 0; i < 16; i++)
					{
						if (counts[i] > best)
						{
							best = counts[i];
							col1 = i;
						}
					}
				}
				else
				{
					col1 = (mOptPaper - 1);
				}

				// reset count of selected color to zero so we don't pick it twice
				counts[col1] = 0;

				best = 0;
				for (i = 0; i < 16; i++)
				{
					if (counts[i] > best)
					{
						best = counts[i];
						col2 = i;
					}
				}

				if (mOptPaper == 0)
				{
					// Make the "brighter" color the ink to make bitmap pretty (if wanted)
					if (mOptAttribOrder == 0 && col2 < col1)
					{
						int tmp = col2;
						col2 = col1;
						col1 = tmp;
					}
				}

				// Final pass on cell, select which of two colors we can use
				for (i = 0; i < cellht; i++)
				{
					for (j = 0; j < 8; j++)
					{
						int loc = (y * cellht + i) * gDevice->mXRes + x * 8 + j;
						gBitmapSpec[loc] = pick_from_2_c64_cols(gBitmapProc[loc], col1, col2);
						mSpectrumBitmapLinear[(y * cellht + i) * (gDevice->mXRes / 8) + x] <<= 1;
						mSpectrumBitmapLinear[(y * cellht + i) * (gDevice->mXRes / 8) + x] |= (gBitmapSpec[loc] == col1 ? 0 : 1);
					}
				}

				// Store the cell's attribute
				mSpectrumAttributes[y * (gDevice->mXRes / 8) + x] = (col2 & 0x7) | ((col1 & 7) << 3) | (((col1 & 8) != 0) << 6);
			}
		}

		// Map color indices to palette
		for (i = 0; i < gDevice->mXRes * gDevice->mYRes; i++)
		{
			gBitmapSpec[i] = gC64Palette[gBitmapSpec[i]] | 0xff000000;
		}
	}

	virtual void savescr(FILE * f)
	{
		int attrib_size_multiplier = 1 << (mOptCellSize);
		unsigned char *bm = mSpectrumBitmapLinear;
		fwrite(bm, (gDevice->mXRes / 8) * gDevice->mYRes, 1, f);
		fwrite(mSpectrumAttributes, (gDevice->mXRes / 8) * (gDevice->mYRes / 8) * attrib_size_multiplier, 1, f);
	}

	virtual void saveh(FILE * f)
	{
		unsigned char *bm = mSpectrumBitmapLinear;

		int attrib_size_multiplier = 1 << (mOptCellSize);
		int i, c = 0;
		for (i = 0; i < (gDevice->mXRes / 8) * gDevice->mYRes; i++)
		{
			fprintf(f, "%3u,", bm[i]);
			c++;
			if (c >= 32)
			{
				fprintf(f, "\n");
				c = 0;
			}
		}
		fprintf(f, "\n\n");
		c = 0;
		for (i = 0; i < (gDevice->mXRes / 8) * (gDevice->mYRes / 8) * attrib_size_multiplier; i++)
		{
			fprintf(f, "%3u%s", mSpectrumAttributes[i], i != ((gDevice->mXRes / 8) * (gDevice->mYRes / 8) * attrib_size_multiplier) - 1 ? "," : "");
			c++;
			if (c >= 32)
			{
				fprintf(f, "\n");
				c = 0;
			}
		}
	}

	virtual void saveinc(FILE * f)
	{
		unsigned char *bm = mSpectrumBitmapLinear;
		
		int attrib_size_multiplier = 1 << (mOptCellSize);
		int i;
		for (i = 0; i < (gDevice->mYRes / 8) * gDevice->mYRes; i++)
		{
			fprintf(f, "\t.db #0x%02x\n", bm[i]);
		}
		fprintf(f, "\n\n");
		for (i = 0; i < (gDevice->mXRes / 8) * (gDevice->mYRes / 8) * attrib_size_multiplier; i++)
		{
			fprintf(f, "\t.db #0x%02x\n", mSpectrumAttributes[i]);
		}
	}

	virtual void attr_bitm()
	{
		int cellht;
		switch (mOptCellSize)
		{
			//case 0:
		default:
			cellht = 8;
			break;
		case 1:
			cellht = 4;
			break;
		case 2:
			cellht = 2;
			break;
		case 3:
			cellht = 1;
			break;
		}

		int i, j;
		for (i = 0; i < gDevice->mYRes; i++)
		{
			for (j = 0; j < gDevice->mXRes; j++)
			{
				int fg, bg;

				int attr = mSpectrumAttributes[(i / cellht) * (gDevice->mXRes / 8) + j / 8];
				fg = gC64Palette[((attr >> 0) & 7) | (((attr & 64)) >> 3)] | 0xff000000;
				bg = gC64Palette[((attr >> 3) & 7) | (((attr & 64)) >> 3)] | 0xff000000;

				gBitmapAttr[i * gDevice->mXRes + j] = (j % 8 < (((8 - cellht) / 2) + i % cellht)) ? fg : bg;
				gBitmapBitm[i * gDevice->mXRes + j] = (mSpectrumBitmapLinear[(i)* (gDevice->mXRes / 8) + j / 8] & (1 << (7 - (j % 8)))) ? 0xffc0c0c0 : 0xff000000;
			}
		}
		update_texture(gTextureAttr, gBitmapAttr);
		update_texture(gTextureBitm, gBitmapBitm);

		ImVec2 picsize((float)gDevice->mXRes, (float)gDevice->mYRes);
		ImGui::Image((ImTextureID)gTextureAttr, picsize, ImVec2(0, 0), ImVec2(gDevice->mXRes / 1024.0f, gDevice->mYRes / 512.0f)); ImGui::SameLine(); ImGui::Text(" "); ImGui::SameLine();
		ImGui::Image((ImTextureID)gTextureBitm, picsize, ImVec2(0, 0), ImVec2(gDevice->mXRes / 1024.0f, gDevice->mYRes / 512.0f)); ImGui::SameLine(); ImGui::Text(" ");
	}

	virtual void options()
	{
		if (ImGui::Combo("Attribute order", &mOptAttribOrder, "Make bitmap pretty\0Make bitmap compressable\0")) gDirty = 1;
		if (ImGui::Combo("Paper attribute", &mOptPaper, "Optimal\0Black\0Blue\0Red\0Purple\0Green\0Cyan\0Yellow\0White\0")) gDirty = 1;
		if (ImGui::Combo("Attribute cell size", &mOptCellSize, "8x8 (standard)\08x4 (bicolor)\08x2\08x1\0")) { gDirty = 1; mOptHeightCells = mXRes / (8 >> mOptCellSize); mXRes = mOptHeightCells * (8 >> mOptCellSize); gDirtyPic = 1; }
		if (ImGui::SliderInt("Bitmap width in cells", &mOptWidthCells, 1, 1028 / 8)) { gDirty = 1; gDirtyPic = 1; mXRes = mOptWidthCells * 8; }
		if (ImGui::SliderInt("Bitmap height in cells", &mOptHeightCells, 1, 512 / (8 >> mOptCellSize))) { gDirty = 1; gDirtyPic = 1; mYRes = mOptHeightCells * (8 >> mOptCellSize); }
		ImGui::Combo("Bitmap order when saving", &mOptScreenOrder, "Linear order\0Spectrum video RAM order\0");
	}

	virtual void zoomed(int aWhich)
	{
		int tex;
		switch (aWhich)
		{
		default: //case 0:
			tex = gTextureSpec;
			break;
		case 1:
			tex = gTextureProc;
			break;
		case 2:
			tex = gTextureOrig;
			break;
		}

		if (gOptZoomStyle == 1)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
			int i, j;
			int cellht = 8 >> mOptCellSize;
			int ymax = (gDevice->mYRes / 8) << mOptCellSize;
			for (i = 0; i < ymax; i++)
			{
				for (j = 0; j < (gDevice->mXRes / 8); j++)
				{
					ImGui::Image(
						(ImTextureID)tex,
						ImVec2(8.0f * gOptZoom, (float)cellht * gOptZoom),
						ImVec2((8 / 1024.0f) * (j + 0), (cellht / (float)gDevice->mYRes) * (i + 0) * (gDevice->mYRes / 512.0f)),
						ImVec2((8 / 1024.0f) * (j + 1), (cellht / (float)gDevice->mYRes) * (i + 1) * (gDevice->mYRes / 512.0f)));

					if (j != (gDevice->mXRes / 8) - 1)
					{
						ImGui::SameLine();
					}
				}
			}
			ImGui::PopStyleVar();
		}
		else
		{
			ImGui::Image(
				(ImTextureID)tex,
				ImVec2((float)gDevice->mXRes * gOptZoom,
				(float)gDevice->mYRes * gOptZoom),
				ImVec2(0, 0),
				ImVec2(gDevice->mXRes / 1024.0f, gDevice->mYRes / 512.0f));
		}
	}

	virtual void writeOptions(FILE *f)
	{
		Modifier::write(f, mOptAttribOrder);
		Modifier::write(f, mOptBright);
		Modifier::write(f, mOptPaper);
		Modifier::write(f, mOptCellSize);
		Modifier::write(f, mOptScreenOrder);
		Modifier::write(f, mOptWidthCells);
		Modifier::write(f, mOptHeightCells);
	}

	virtual void readOptions(FILE *f)
	{
		Modifier::read(f, mOptAttribOrder);
		Modifier::read(f, mOptBright);
		Modifier::read(f, mOptPaper);
		Modifier::read(f, mOptCellSize);
		Modifier::read(f, mOptScreenOrder);
		Modifier::read(f, mOptWidthCells);
		Modifier::read(f, mOptHeightCells);

		mXRes = mOptWidthCells * 8;
		mYRes = mOptHeightCells * (8 >> mOptCellSize);

		gDirty = 1;
		gDirtyPic = 1;
	}
};