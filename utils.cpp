#include "utils.h"

using namespace std;

template<typename T>
void ReleaseVec(std::vector<T>&input)
{
	std::vector<T>().swap(input);
}
template<typename T>
void ScaleVec(std::vector<T>&input)
{
	std::vector<T>(input).swap(input);
}


void _MyData::_DataLoading(const string& path, const string& type, _MyData* md)
{
	static int path_index = 0; //用来修改样本标签的
	static int class_index = -1;
	struct _finddata_t   filefind;
	string  curr = path + "\\*";
	int  handle;
	if ((handle = _findfirst(curr.c_str(), &filefind)) == -1)return;
	std::vector<std::string>traverse_results;
	while (_findnext(handle, &filefind) == 0)
	{
		if (!strcmp(filefind.name, ".."))
		{
			continue;
		}
		if ((_A_SUBDIR == filefind.attrib)) //是目录  
		{
			path_index += 1;
			vector<string>pth;
			md->_imagsPath.insert(pair<string, vector<string>>(filefind.name, pth));
			curr = path + "\\" + filefind.name;
			string temp = filefind.name;
			md->_dname.push_back(temp);
			_DataLoading(curr, type, md);
			class_index += 2;
			
		}
		else//不是目录，是文件       
		{
			string temp = filefind.name;
			int location = temp.find_first_of('.');
			string substr = temp.substr(location + 1, temp.size() - location - 1);
			if (substr == type)
			{
				md->_imagsPath[md->_dname[path_index - 1]].push_back(path + "\\" + filefind.name);
				md->_labels[md->_dname[path_index - 1]].push_back(class_index);
			}
			if (substr == "pts")
			{
				cv::Mat_<float> shape = _ReadPts(path + "\\" + filefind.name, "pts", 51);
				md->_gtShape.push_back(shape);
			}	
		}
	}
	_findclose(handle);
}
cv::Mat_<float>  _MyData::_ReadPts(const string& ptsName, const string& type, const int npt)
{
	cv::Mat_<float> shape(npt, 2);
	string flag;

	ifstream ifs(ptsName);
	string point_str;
	string point_x, point_y;
	float pointx, pointy;
	int is_number = 0;
	int pt_index = 0;
	int lineflag = 0;

	while (getline(ifs, point_str))
	{
		if (lineflag == 0)
		{
			if ('0' > point_str.at(0) || point_str.at(0) > '9')
				flag = "pts";
			else
				flag = "ptx";
			lineflag += 1;
		}

		if (point_str.at(0) == '}')
			break;
		if ('0' > point_str.at(0) || point_str.at(0) > '9')
		{
			if (is_number < 3)
			{
				is_number += 1;
				continue;
			}
		}
		int index = 0;
		for (uint32 j = 0; j < point_str.size(); j++)
		{
			if (flag == "pts")
			{
				if (point_str.at(j) != ' ')
					index = j + 1;
				else
					break;
			}
			if (flag == "ptx")
			{
				if (point_str.at(j) != '\t')
					index = j + 1;
				else
					break;
			}
		}

		if (index == point_str.size())
			continue;

		point_x = point_str.substr(0, index);
		point_y = point_str.substr(index + 1, point_str.size() - index);

		//std::cout << point_str<<endl;
		istringstream iss(point_x);
		iss >> pointx;
		iss = istringstream(point_y);
		iss >> pointy;
		switch (npt)
		{
		case 68:
			shape(pt_index, 0) = pointx;
			shape(pt_index, 1) = pointy;
			break;
		case 51:
			if ((pt_index) >= 17)
			{
				shape(pt_index - 17, 0) = pointx;
				shape(pt_index - 17, 1) = pointy;
			}
			break;
		}
		pt_index += 1;
		is_number += 1;
	}
	ifs.close();
	return shape;
}
_MyData::~_MyData()
{
	ReleaseVec<string>(_dname);
	ReleaseVec<cv::Mat_<float>>(_gtShape);
	_imagsPath.clear();
	_labels.clear();
}
void _MyData::_GetBbox(const vector<cv::Mat_<float>>& shape, const cv::Scalar_<float>& factor, vector<_BBOX>& bbox_origial)
{
	std::vector<cv::Rect>faces;
	_BBOX validBbox;
	for (uint16 i = 0; i < shape.size(); i++)
	{
		faces.clear();
		bool result = true;

		maxminVec(shape[i], validBbox);
		validBbox.x -= factor(0)*validBbox.height;
		validBbox.y -= factor(1)*validBbox.width;
		validBbox.width += (2)*validBbox.height;
		validBbox.height = validBbox.width;
		validBbox.ctx = validBbox.x + validBbox.width / 2;
		validBbox.cty = validBbox.y + validBbox.height / 2;
		bbox_origial.push_back(validBbox);
	}
}
void _MyData::_CalcMeanshape()
{
	_Meanshape.create(_gtShape[0].rows , 2);
	for (int i = 0; i < _gtShape.size(); i++)
		_Meanshape += ProjectShape(_gtShape[i].col(0), _gtShape[i].col(1), _bbox_origial[i]);
	_Meanshape = 1.0 / _gtShape.size()*_Meanshape;
}




void drawFeatureP(cv::Mat& image, const cv::Rect& faceRegion, const cv::Mat_<float>&gtp, cv::Scalar sc)
{
	for (int i = 0; i < gtp.rows; i++)
	{
		for (int j = 0; j < gtp.cols / 2; j++)
			cv::circle(image, cv::Point2d(gtp(i, 2 * j), gtp(i, 2 * j + 1)), 3, sc);
	}
	if (faceRegion.width != 0)
		cv::rectangle(image, faceRegion, cv::Scalar(255, 0, 0));
}

void maxminVec(const cv::Mat_<float>& shape, _BBOX& wh)
{
	double* min_x = (double*)malloc(sizeof(double) * 2);
	double* max_x = (double*)malloc(sizeof(double) * 2);
	double* min_y = (double*)malloc(sizeof(double) * 2);
	double* max_y = (double*)malloc(sizeof(double) * 2);
	minMaxIdx(shape.col(0), min_x, max_x);
	minMaxIdx(shape.col(1), min_y, max_y);
	float w = max_x[0] - min_x[0];
	float h = max_y[0] - min_y[0];

	wh.x = min_x[0]; wh.y = min_y[0]; wh.width = w; wh.height = h;

	if (wh.width > wh.height)
	{
		wh.y -= abs(wh.width - wh.height) / 2;
		wh.height = wh.width;
	}
	else if (wh.height > wh.width)
	{
		wh.x -= abs(wh.width - wh.height) / 2;
		wh.width = wh.height;
	}
	wh.ctx = wh.x + wh.width / 2;
	wh.cty = wh.y + wh.height / 2;

	free(min_x); free(min_y); free(max_x); free(max_y);
}

int randScalar(const int max, int* input)
{
	for (int i = 0; i <= max; ++i) input[i] = i;
	for (int i = max; i >= 1; --i) std::swap(input[i], input[rand() % i]);
	return input[0];
}


void normeImage(const cv::Mat& image, cv::Mat_<float>& bbox, cv::Mat& normedImage, const int sizeNormed, cv::Point2f& bp, cv::Point2f& scaleFac)
{
	//	int w = image.cols, h = image.rows;
	//	assert(image.channels() == 1);
	//	cv::Mat extendedImage = image.clone();
	//	float borderLeft = 0.5*image.cols; // x and y are patch-centers
	//	float borderTop = 0.5*image.rows;
	//	float borderRight = 0.5*image.cols;
	//	float borderBottom = 0.5*image.rows;
	//	copyMakeBorder(extendedImage, extendedImage, borderTop, borderBottom, borderLeft, borderRight, cv::BORDER_REPLICATE);
}
cv::Mat_<float> ProjectShape(const cv::Mat_<float>& x, const cv::Mat_<float>& y, const _BBOX& bbox)
{
	cv::Mat_<float> results(x.rows, 2);
	for (int i = 0; i < x.rows; i++)
	{
		results(i, 0) = (x(i, 0) - bbox.ctx) / (bbox.width / 2.0);
		results(i, 1) = (y(i, 0) - bbox.cty) / (bbox.height / 2.0);
	}
	return results;
}
cv::Mat_<float> ReProjection(const cv::Mat_<float>& meanShape, const _BBOX& bbox, const cv::Scalar_<float> factor)
{
	cv::Mat_<double> results(meanShape.rows, 2);
	for (int i = 0; i < meanShape.rows; i++){
		results(i, 0) = factor(0)* meanShape(i, 0)*bbox.width / 2.0 + bbox.ctx;
		results(i, 1) = factor(1)*meanShape(i, 1)*bbox.height / 2.0 + bbox.cty;
	}
	return results;
}
void DrawPredictImage(cv::Mat& image, cv::Mat_<float>& shape)
{
	for (int i = 0; i < shape.rows; i++)
		cv::circle(image, cv::Point2f(shape(i, 0), shape(i, 1)), 1, cv::Scalar(0,255,0),-1);
}
cv::Mat_<float> calcRME(const std::vector<cv::Mat_<float>>&X_updated, const cv::Mat_<float>&gtp_x, const cv::Mat_<float>&gtp_y, int * left_eye, int* right_eye, const int numRbbox, const int numpt)
{
	//以下计算误差
	cv::Mat_<float> err = cv::Mat(1, X_updated.size(), CV_32FC1);
	double mean_error = 0;
	float cent_x_l[6] = {};
	float cent_y_l[6] = {};
	float cent_x_r[6] = {};
	float cent_y_r[6] = {};

	for (int i = 0; i < X_updated.size(); i++)
	{
		double sample_error_L2 = 0;

		float inter_ocular_distance = 0;
		
		for (int it = 0; it < 6; it++)
		{
			if (numpt == 68)
			{
				cent_x_l[it] = gtp_x.col(i / numRbbox)((left_eye[it] - 1) );
				cent_y_l[it] = gtp_y.col(i / numRbbox)((left_eye[it] - 1));
				cent_x_r[it] = gtp_x.col(i / numRbbox)((right_eye[it] - 1));
				cent_y_r[it] = gtp_y.col(i / numRbbox)((right_eye[it] - 1));
			}
			else
			{
				cent_x_l[it] = gtp_x.col(i / numRbbox)((left_eye[it] - 1 - 17));
				cent_y_l[it] = gtp_y.col(i / numRbbox)((left_eye[it] - 1 - 17));
				cent_x_r[it] = gtp_x.col(i / numRbbox)((right_eye[it] - 1 - 17));
				cent_y_r[it] = gtp_y.col(i / numRbbox)((right_eye[it] - 1 - 17));
			}

		}
		float m_x_l = 0, m_y_l = 0, m_x_r = 0, m_y_r = 0;

		for (int m = 0; m < 6; m++)
		{
			m_x_l += cent_x_l[m] / 6;
			m_x_r += cent_x_r[m] / 6;
			m_y_l += cent_y_l[m] / 6;
			m_y_r += cent_y_r[m] / 6;
		}
		inter_ocular_distance = sqrt((m_x_r - m_x_l)*(m_x_r - m_x_l) + (m_y_r - m_y_l)*(m_y_r - m_y_l));

		for (int j = 0; j < numpt; j++)
			sample_error_L2 += (gtp_x(j, i / numRbbox) - X_updated[i](j, 0))*(gtp_x(j, i / numRbbox) - X_updated[i](j, 0)) + (gtp_y(j, i / numRbbox) - X_updated[i](j, 1))*(gtp_y(j, i / numRbbox) - X_updated[i](j, 1));
		std::cout << "单个样本的RMSE为: " << std::to_string(i) << "error(/pixel) :" << (100 * sqrt(sample_error_L2 / numpt) / inter_ocular_distance) << std::endl;
		mean_error += (100 * sqrt(sample_error_L2 / numpt) / inter_ocular_distance);

		err(i) = (100 * sqrt(sample_error_L2 / numpt) / inter_ocular_distance);
	}
	std::cout << "在整个样本集上的RMSE为: " << "（像素为单位） :" << mean_error / X_updated.size() << endl;
	return err;
}
cv::Mat_<float> calcRME(const std::vector<cv::Mat_<float>>&X_updated, const cv::Mat_<float>&gtp_x, const cv::Mat_<float>&gtp_y, int * left_eye, int* right_eye, const int numRbbox, const int numpt ,const cv::Mat_<float>mask)
{
	//以下计算误差
	cv::Mat_<float> err = cv::Mat(1, mask.cols, CV_32FC1);
	double mean_error = 0;
	float cent_x_l[6] = {};
	float cent_y_l[6] = {};
	float cent_x_r[6] = {};
	float cent_y_r[6] = {};

	int acc = 0;
	for (int i = 0; i < X_updated.size(); i++)
	{
		if (mask(acc) <= 10)
		{
			double sample_error_L2 = 0;

			float inter_ocular_distance = 0;

			for (int it = 0; it < 6; it++)
			{
				if (numpt == 68)
				{
					cent_x_l[it] = gtp_x.col(i / numRbbox)((left_eye[it] - 1));
					cent_y_l[it] = gtp_y.col(i / numRbbox)((left_eye[it] - 1));
					cent_x_r[it] = gtp_x.col(i / numRbbox)((right_eye[it] - 1));
					cent_y_r[it] = gtp_y.col(i / numRbbox)((right_eye[it] - 1));
				}
				else
				{
					cent_x_l[it] = gtp_x.col(i / numRbbox)((left_eye[it] - 1 - 17));
					cent_y_l[it] = gtp_y.col(i / numRbbox)((left_eye[it] - 1 - 17));
					cent_x_r[it] = gtp_x.col(i / numRbbox)((right_eye[it] - 1 - 17));
					cent_y_r[it] = gtp_y.col(i / numRbbox)((right_eye[it] - 1 - 17));
				}

			}
			float m_x_l = 0, m_y_l = 0, m_x_r = 0, m_y_r = 0;

			for (int m = 0; m < 6; m++)
			{
				m_x_l += cent_x_l[m] / 6;
				m_x_r += cent_x_r[m] / 6;
				m_y_l += cent_y_l[m] / 6;
				m_y_r += cent_y_r[m] / 6;
			}
			inter_ocular_distance = sqrt((m_x_r - m_x_l)*(m_x_r - m_x_l) + (m_y_r - m_y_l)*(m_y_r - m_y_l));

			for (int j = 0; j < numpt; j++)
				sample_error_L2 += (gtp_x(j, i / numRbbox) - X_updated[acc](j, 0))*(gtp_x(j, i / numRbbox) - X_updated[acc](j, 0)) + (gtp_y(j, i / numRbbox) - X_updated[acc](j, 1))*(gtp_y(j, i / numRbbox) - X_updated[acc](j, 1));
			std::cout << "单个样本的RMSE为: " << std::to_string(i) << "error(/pixel) :" << (100 * sqrt(sample_error_L2 / numpt) / inter_ocular_distance) << std::endl;
			mean_error += (100 * sqrt(sample_error_L2 / numpt) / inter_ocular_distance);

			err(acc) = (100 * sqrt(sample_error_L2 / numpt) / inter_ocular_distance);
		}

	}
	std::cout << "在整个样本集上的RMSE为: " << "（像素为单位） :" << mean_error / err.cols << endl;
	return err;
}