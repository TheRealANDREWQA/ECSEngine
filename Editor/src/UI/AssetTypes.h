#pragma once

enum class AssetType : unsigned char {
	Directory,
	Texture,
	Model,
	Material,
	AudioClip,
	SourceFile,
	Prefab,
	Count
};