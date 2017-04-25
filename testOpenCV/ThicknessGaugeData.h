#pragma once


class ThicknessGaugeData {
	
protected:

	cv::Mat planarImage_;

	// The pixels located in the image
	vi pixels_;

	vd allPixels_;
	vd measureLine_;
	vd rightSideLine_;
	vd leftSideLine_;
	vd intensity_;

	vector<p> lines_;
	p center_;


public:
	void setImageType(int imageType);
protected:
	cv::Size imageSize_;
	int imageType_;

	double rightMean_;
	double leftMean_;

public:
	int getImageType() const;
	void setImageSize(cv::Size size);
protected:
	const int pixelChunkCount_ = 16;
	const int pixelChunkSize_ = 153; // lowest whole number from 2448 as (2448 >> 4 = 153)


public:

	ThicknessGaugeData(): imageType_(0), rightMean_(0), leftMean_(0) { }

	cv::Mat& GetPlanarImage();

	void setPlanarImage(const cv::Mat& mat);
	void setRightMean(double mean);
	double getRightMean() const;
	void setLeftMean(double mean);
	double getLeftMean() const;

	const vi& getPixels() const;
	const vd& getAllPixels() const;
	const vd& getMeasureLine() const;
	const vd& getRightSideLine() const;
	const vd& getLeftSideLine() const;
	const vd& getIntensity() const;

	double getIntensity(int x) const;

};

inline void ThicknessGaugeData::setRightMean(double mean) {
	rightMean_ = mean;
}

inline double ThicknessGaugeData::getRightMean() const {
	return rightMean_;
}

inline void ThicknessGaugeData::setLeftMean(double mean) {
	leftMean_ = mean;
}

inline double ThicknessGaugeData::getLeftMean() const {
	return leftMean_;
}

inline void ThicknessGaugeData::setImageType(int imageType) {
	imageType_ = imageType;
}

inline int ThicknessGaugeData::getImageType() const {
	return imageType_;
}

inline void ThicknessGaugeData::setImageSize(cv::Size size) {
	imageSize_ = size;
}

inline const vi& ThicknessGaugeData::getPixels() const {
	return pixels_;
}

inline const vd& ThicknessGaugeData::getAllPixels() const {
	return allPixels_;
}

inline const vd& ThicknessGaugeData::getMeasureLine() const {
	return measureLine_;
}

inline const vd& ThicknessGaugeData::getRightSideLine() const {
	return rightSideLine_;
}

inline const vd& ThicknessGaugeData::getLeftSideLine() const {
	return leftSideLine_;
}

inline const vd& ThicknessGaugeData::getIntensity() const {
	return intensity_;
}

inline double ThicknessGaugeData::getIntensity(int x) const {
	return intensity_.at(x).y;
}

inline cv::Mat& ThicknessGaugeData::GetPlanarImage() {
	return planarImage_;
}

inline void ThicknessGaugeData::setPlanarImage(const Mat& mat) {
	planarImage_ = mat;
}