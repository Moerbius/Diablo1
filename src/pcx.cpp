#include "pcx.hpp"

#include "storm/stormlib.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

#pragma pack(push, 1)
struct PCXHeader {
	std::uint8_t manufacturer;
	std::uint8_t version;
	std::uint8_t encoding;
	std::uint8_t bitsPerPixel;
	std::uint16_t xmin;
	std::uint16_t ymin;
	std::uint16_t xmax;
	std::uint16_t ymax;
	std::uint16_t hDPI;
	std::uint16_t vDPI;
	std::uint8_t colormap[48];
	std::uint8_t reserved;
	std::uint8_t numPlanes;
	std::uint16_t bytesPerLine;
	std::uint16_t paletteInfo;
	std::uint16_t screenWidth;
	std::uint16_t screenHeight;
	std::uint8_t filler[54];
};
#pragma pack(pop)

PCXImage PCX::ParsePCXData(const std::vector<std::byte>& data)
{
	PCXImage result{};

	if (data.size() < 128) {
		std::fprintf(stderr, "PCX: Data too small to contain header\n");
		return result;
	}

	const PCXHeader* header = reinterpret_cast<const PCXHeader*>(data.data());

	if (header->manufacturer != 0x0A) {
		std::fprintf(stderr, "PCX: Invalid manufacturer byte: 0x%02X\n", header->manufacturer);
		return result;
	}

	result.width = header->xmax - header->xmin + 1;
	result.height = header->ymax - header->ymin + 1;

	// For 8-bit color (256 colors), PCX uses 1 plane of 8 bits
	if (header->numPlanes != 1 || header->bitsPerPixel != 8) {
		std::fprintf(stderr, "PCX: Only 8-bit single-plane format supported (got %d planes, %d bits)\n",
			header->numPlanes, header->bitsPerPixel);
		return result;
	}

	std::vector<std::uint8_t> scanline;
	scanline.reserve(header->bytesPerLine);
	result.pixels.reserve(result.width * result.height);

	// Decode RLE-compressed image data
	std::size_t pos = 128;  // Skip header
	for (std::uint32_t row = 0; row < result.height && pos < data.size(); ++row) {
		scanline.clear();

		while (scanline.size() < header->bytesPerLine && pos < data.size()) {
			std::uint8_t byte = static_cast<std::uint8_t>(data[pos++]);

			// RLE marker: high 2 bits are set
			if ((byte & 0xC0) == 0xC0) {
				std::uint8_t count = byte & 0x3F;
				if (pos >= data.size()) break;
				std::uint8_t value = static_cast<std::uint8_t>(data[pos++]);
				for (std::uint8_t i = 0; i < count && scanline.size() < header->bytesPerLine; ++i) {
					scanline.push_back(value);
				}
			} else {
				scanline.push_back(byte);
			}
		}

		// Copy scanline pixels (only first width bytes)
		for (std::uint32_t i = 0; i < result.width && i < scanline.size(); ++i) {
			result.pixels.push_back(scanline[i]);
		}
	}

	// Load palette (256 colors * 3 bytes = 768 bytes at end of file)
	if (data.size() >= 768) {
		const std::uint8_t* paletteData = reinterpret_cast<const std::uint8_t*>(data.data() + data.size() - 768);
		result.palette.assign(paletteData, paletteData + 768);
	}

	return result;
}

PCXImage PCX::LoadFromMPQ(StormLib& mpq, const std::string& filename)
{
	std::vector<std::byte> data;
	if (!mpq.ReadFile(filename, data)) {
		return PCXImage{};
	}

	return ParsePCXData(data);
}

std::vector<std::uint32_t> PCX::ConvertToRGBA32(const PCXImage& image)
{
	std::vector<std::uint32_t> rgba;
	rgba.reserve(image.pixels.size());

	// Find the transparent color by checking the top-left corner
	// Diablo typically uses the color at the top-left as the transparent key
	std::uint32_t transparentIndex = 0;
	if (!image.pixels.empty()) {
		transparentIndex = static_cast<std::uint32_t>(image.pixels[0]);
	}

	for (std::uint8_t pixelIndex : image.pixels) {
		std::uint32_t paletteOffset = static_cast<std::uint32_t>(pixelIndex) * 3;

		std::uint32_t r = 0, g = 0, b = 0;
		if (paletteOffset + 2 < image.palette.size()) {
			r = image.palette[paletteOffset + 0];
			g = image.palette[paletteOffset + 1];
			b = image.palette[paletteOffset + 2];
		}

		std::uint32_t pixelValue = (static_cast<std::uint32_t>(r) << 0) |
		                          (static_cast<std::uint32_t>(g) << 8) |
		                          (static_cast<std::uint32_t>(b) << 16) |
		                          (255u << 24);
		
		// Make the top-left color transparent (standard Diablo method)
		if (pixelIndex == transparentIndex) {
			pixelValue = 0u;
		}

		rgba.push_back(pixelValue);
	}

	return rgba;
}
