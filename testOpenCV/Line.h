#pragma once
#include <vector>
#include "PointData.h"
#include <map>
#include "Util.h"
#include <opencv2/stitching/detail/warpers.hpp>

/**
 * \brief Line class, contains information from a singular captured frame.
 * Including defined sections for 3 sides (right/center/left), frame reference and test output matrix.
 */
class Line {
public:
	enum class Location {
		baseOne, baseTwo, heigthOne, heigthTwo
	};

private:

	enum class SortMethod {
		none, x, y
	};

	struct elementYsort {
		bool operator()(cv::Point2i pt1, cv::Point2i pt2) const { return pt1.y < pt2.y; }
	} sortY;

	struct elementXsort {
		bool operator()(cv::Point2i pt1, cv::Point2i pt2) const { return pt1.x < pt2.x; }
	} sortX;

	const std::map<Location, int> locationBaseMap = { { Location::baseOne , 0 }, { Location::baseTwo, 1 } };
	const std::map<Location, int> locationHeigthMap = { { Location::heigthOne , 0 }, { Location::heigthTwo, 1 } };

	std::map<int, unsigned char> intensity_;


public:
	const std::map<int, unsigned char>& intensity() const {
		return intensity_;
	}

	void intensity(const std::map<int, unsigned char>& intensity) {
		intensity_ = intensity;
	}

private:
	/**
	* \brief The frame for which the data set in the class is based
	*/
	cv::Mat frame_;

	/**
	* \brief Matrix representation of the data vectors in the class
	*/
	cv::Mat output_;

	/**
	* \brief Area of interest based on the 4 "focus" areas
	*/
	cv::Rect roi[4];

	/**
	* \brief All the sparse elements
	*/
	std::vector<cv::Point2i> allSparse_;

	/**
	* \brief All the sparse elements with differentiated intensity values
	*/
	std::vector<cv::Point2i> allTotal_;

	/**
	* \brief Left side of the elements
	*/
	std::vector<cv::Point2i> leftOne_;

	/**
	* \brief Left side #2 of the elements
	*/
	std::vector<cv::Point2i> leftTwo_;

	/**
	* \brief Right side #1 of the elements
	*/
	std::vector<cv::Point2i> rightOne_;

	/**
	* \brief Right side #2 of the elements
	*/
	std::vector<cv::Point2i> rightTwo_;

	/**
	* \brief The calculated baseline for all 3 sides
	*/
	double baseLine_[2] = { 0.0, 0.0 };

	double heigthLin_[2] = { 0.0, 0.0 };


public:


public:

	/**
	 * \brief Differentiates points in a vector
	 * \param input The vector to be differentiated
	 * \param output The results
	 */
	static void differentiateY(std::vector<cv::Point2i>& input, std::vector<cv::Point2i>& output);

	/**
	 * \brief Differentiates the intensity levels in X direction
	 */
	void differentiateIntensity();

	/**
	 * \brief Merges the differentiated values of height and intensity into Y
	 */
	void mergeIntensity();

	/**
	 * \brief Combines two vectors into a third
	 * \param sourceOne The first vector
	 * \param sourceTwo The second vector
	 * \param target The target vector with sourceOne and sourceTwo data
	 * \param sortX Sorting method to be used when combining is done
	 */
	void combine(std::vector<cv::Point2d>& sourceOne, std::vector<cv::Point2d>& sourceTwo, std::vector<cv::Point2d> target, SortMethod sortX) const;

	/**
	 * \brief Get the pixel intensity of a location from the current frame
	 * \param location The point location to get the intensity from
	 * \return The grey scale pixel intensity
	 */
	unsigned char getPixelIntensity(cv::Point2d& location);

	/**
	 * \brief Generates the output matrix based on the current elements
	 */
	void generateOutput();

	bool generateSparse();

public:

	/**
	 * \brief Splits the elements based on values in X,
	 * <rigth> < rightX, <center> < leftX, the rest in <left>
	 * \param rightOne The right section border in X
	 * \param rightTwo The left section border in X
	 */
	void split(double leftOne, double leftTwo, double rightOne, double rightTwo);

public: // getters and setter + minor functions

	/**
	 * \brief Reset the default output matrix
	 */
	void resetOutput() {
		resetOutput(frame_);
	}

	/**
	 * \brief Reset the default output matrix using custom matrix as template
	 * \param templateFrame The template to base the configuration of the output matrix on
	 */
	void resetOutput(cv::Mat& templateFrame) {
		output_ = cv::Mat::zeros(templateFrame.rows, templateFrame.cols, templateFrame.type());
	}

	/**
	 * \brief Set the class main frame reference (no pun)
	 * \param frameToSet The frame t
	 */
	void setFrame(cv::Mat& frameToSet) {
		frameToSet.copyTo(frame_); // copy ref
	}

	/**
	 * \brief Get output matrix reference
	 * \return The output matrix reference
	 */
	const cv::Mat& getOutput() const {
		return output_;
	}

	/**
	 * \brief Get the baseline (Y)
	 * \param location For which location
	 * \return The baseline (Y)
	 */
	double getLine(Location location) {
		switch (location) {
		case Location::baseOne:;
		case Location::baseTwo:
			return baseLine_[locationBaseMap.at(location)];
			break;
		case Location::heigthOne:;
		case Location::heigthTwo:;
		default:;
			return heigthLin_[locationHeigthMap.at(location)];
		}
	}

};

inline void Line::differentiateY(std::vector<cv::Point2i>& input, std::vector<cv::Point2i>& output) {

	output.clear();

	if (input.empty())
		return;

	auto size = input.size();

	if (size == 1) {
		output.push_back(cv::Point(input.front().x, -input.front().y));
		return;
	}

	output.reserve(input.size() - 1);

	for (auto i = 1; i < size; ++i)
		output.push_back(cv::Point(input[i].x, input[i].y - input[i - 1].y));
}

inline void Line::differentiateIntensity() {

	if (frame_.empty())
		return;

	if (allSparse_.empty())
		return;

	auto size = allSparse_.size();

	intensity_.clear();

	if (size == 1) {
		auto front = allSparse_.front();
		intensity_[front.x] = frame_.at<unsigned char>(front);
	}

	for (auto i = 1; i < size; ++i)
		intensity_[allSparse_[i].x] = frame_.at<unsigned char>(allSparse_[i]) - frame_.at<unsigned char>(allSparse_[i - 1]);

}

inline void Line::mergeIntensity() {

	if (allSparse_.empty())
		return;

	if (intensity_.empty())
		return;

	allTotal_.clear();

	allTotal_.reserve(allSparse_.size());

	for (auto& p : allSparse_)
		allTotal_.push_back(cv::Point(p.x, p.y + intensity_.at(p.x)));

}

inline void Line::combine(std::vector<cv::Point2d>& sourceOne, std::vector<cv::Point2d>& sourceTwo, std::vector<cv::Point2d> target, SortMethod sort) const {
	target.reserve(sourceOne.size() + sourceTwo.size());
	target.insert(target.begin(), sourceOne.begin(), sourceOne.end());
	target.insert(target.end(), sourceTwo.begin(), sourceTwo.end());
	if (sort == SortMethod::x)
		std::sort(target.begin(), target.end(), sortX);
	else if (sort == SortMethod::y)
		std::sort(target.begin(), target.end(), sortY);
}

inline unsigned char Line::getPixelIntensity(cv::Point2d& location) {
	if (frame_.empty())
		return 0;

	if (frame_.cols > location.x)
		return 0;

	if (frame_.rows > location.y)
		return 0;

	return frame_.at<uchar>(location);
}

inline void Line::generateOutput() {
	// just basic method, can be optimized.
	for (auto& e : allSparse_)
		output_.at<unsigned char>(e) = 255;
}

inline bool Line::generateSparse() {

	if (!allSparse_.empty())
		allSparse_.clear();

	allSparse_.reserve(frame_.cols);

	std::vector<cv::Point2i> pix;

	pix.reserve(frame_.cols);

	cv::findNonZero(frame_, pix);

	// sort the list in X
	std::sort(pix.begin(), pix.end(), sortX);

	auto x = 0;
	auto y = 0;
	auto count = 0;
	auto highest = 0;

	x = pix.front().x;

	for (auto& p : pix) {
		if (p.x != x) {
			if (count > 0) {
				allSparse_.push_back(cv::Point(x, y));
				count = 0;
			}
			highest = 0;
		}
		auto intensity = frame_.at<unsigned char>(p);
		if (intensity >= highest) {
			highest = p.y;
			x = p.x;
			y = p.y;
		}
		count++;
	}

	return allSparse_.empty() ^ true;

}

inline void Line::split(double leftOne, double leftTwo, double rightOne, double rightTwo) {
	if (allSparse_.empty())
		return;

	// TODO : Validate input.

	auto size = allSparse_.size();

	rightOne_.clear();
	leftOne_.clear();

	rightOne_.reserve(size);
	leftOne_.reserve(size);

	for (auto& p : allSparse_) {
		if (p.x < leftOne)
			leftOne_.push_back(p);
		else if (p.x < leftTwo)
			leftTwo_.push_back(p);
		else if (p.x < rightOne)
			rightOne_.push_back(p);
		else
			rightTwo_.push_back(p);
	}
}
