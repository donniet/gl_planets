#ifndef __TEXTURE_HPP__
#define __TEXTURE_HPP__


#include <stb/stb_image.h>
#include <string>

using std::string;

class Texture {
private:
	string path_;
	int width_;
	int height_;
	int channels_;
	unsigned char * data_;

public:
	Texture(string path, int desired_channels = 3) : path_(path) {
		data_ = stbi_load(path_.c_str(), &width_, &height_, &channels_, desired_channels);
	}
	Texture(Texture && rhs) 
		: path_(rhs.path_), width_(rhs.width_), height_(rhs.height_), 
		  channels_(rhs.channels_), data_(rhs.data_)
	{
		rhs.data_ = nullptr;
	}
	Texture(Texture const & rhs) = delete;

	~Texture() {
		if(is_valid()) {
			stbi_image_free(data_);
			data_ = nullptr;
		}
	}

	bool is_valid() const { return data_ != nullptr; }
	operator bool() const { return is_valid(); }
	unsigned char * data() const { return data_; }
	int width() const { return width_; } 
	int height() const { return height_; }
};


#endif