#include <algorithm>
#include <vector>

#include "caffe/layer.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/vision_layers.hpp"

namespace caffe {

template <typename Dtype>
void SoftmaxPairLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  softmax_pair_axis_ =
      bottom[0]->CanonicalAxisIndex(this->layer_param_.softmax_pair_param().axis());
  top[0]->ReshapeLike(*bottom[0]);
  vector<int> mult_dims(1, bottom[0]->shape(softmax_pair_axis_));
  sum_multiplier_.Reshape(mult_dims);
  Dtype* multiplier_data = sum_multiplier_.mutable_cpu_data();
  caffe_set(sum_multiplier_.count(), Dtype(1), multiplier_data);
  vector<int> scale_dims = bottom[0]->shape();
  scale_dims[softmax_pair_axis_] = 1;
  scale_.Reshape(scale_dims);
}

template <typename Dtype>
void SoftmaxPairLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = top[0]->mutable_cpu_data();
  Dtype* scale_data = scale_.mutable_cpu_data();
  int num = bottom[0]->num();
  int dim = bottom[0]->count() / bottom[0]->num();
  caffe_copy(bottom[0]->count(), bottom_data, top_data);
  for (int t = 0; t < dim/2; ++t) {
      for (int i = 0; i < num; ++i) {
        scale_data[i] = std::max(bottom_data[i*dim+t*2], bottom_data[i*dim+t*2+1]);
      }
      Dtype* bottom_data_pair = new Dtype [num*2];
      for (int i = 0; i < num; ++i) {
        bottom_data_pair[i*2] = bottom_data[i*dim+t*2];
        bottom_data_pair[i*2+1] = bottom_data[i*dim+t*2+1];
      }
      Dtype* top_data_pair = new Dtype [num*2];
      caffe_copy(num*2, bottom_data_pair, top_data_pair);
      // subtraction
      caffe_cpu_gemm<Dtype>(CblasNoTrans, CblasNoTrans, num, 2, 1, -1.,
        scale_data, sum_multiplier_.cpu_data(), 1., top_data_pair);
      // Perform exponentiation
      caffe_exp<Dtype>(num * 2, top_data_pair, top_data_pair);
      // sum after exp
      caffe_cpu_gemv<Dtype>(CblasNoTrans, num, 2, 1., top_data_pair,
          sum_multiplier_.cpu_data(), 0., scale_data);
      // Do division
      for (int i = 0; i < num; ++i) {
        caffe_scal<Dtype>(2, Dtype(1.) / scale_data[i], top_data_pair + i * 2);
        top_data[i*dim+t*2] = top_data_pair[i*2];
        top_data[i*dim+t*2+1] = top_data_pair[i*2+1];
      }
      delete []bottom_data_pair;
      delete []top_data_pair;
  }
}

template <typename Dtype>
void SoftmaxPairLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
  const Dtype* top_diff = top[0]->cpu_diff();
  const Dtype* top_data = top[0]->cpu_data();
  Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
  Dtype* scale_data = scale_.mutable_cpu_data();
  int num = top[0]->num();
  int dim = top[0]->count() / top[0]->num();
  caffe_copy(top[0]->count(), top_diff, bottom_diff);
  for (int t = 0; t < dim/2; ++t) {
      Dtype* top_diff_pair = new Dtype [num*2];
      for (int i = 0; i < num; ++i) {
        top_diff_pair[i*2] = top_diff[i*dim+t*2];
        top_diff_pair[i*2+1] = top_diff[i*dim+t*2+1];
      }
      Dtype* top_data_pair = new Dtype [num*2];
      for (int i = 0; i < num; ++i) {
        top_data_pair[i*2] = top_data[i*dim+t*2];
        top_data_pair[i*2+1] = top_data[i*dim+t*2+1];
      }
      Dtype* bottom_diff_pair = new Dtype [num*2];
      caffe_copy(num*2, top_diff_pair, bottom_diff_pair);
      // Compute inner1d(top_diff, top_data) and subtract them from the bottom diff
      for (int i = 0; i < num; ++i) {
        scale_data[i] = caffe_cpu_dot<Dtype>(2, top_diff_pair + i * 2,
            top_data_pair + i * 2);
      }
      // subtraction
      caffe_cpu_gemm<Dtype>(CblasNoTrans, CblasNoTrans, num, 2, 1, -1.,
          scale_data, sum_multiplier_.cpu_data(), 1., bottom_diff_pair);
      // elementwise multiplication
      caffe_mul<Dtype>(num*2, bottom_diff_pair, top_data_pair, bottom_diff_pair);
      for (int i = 0; i < num; ++i) {
        bottom_diff[i*dim+t*2] = bottom_diff_pair[i*2];
        bottom_diff[i*dim+t*2+1] = bottom_diff_pair[i*2+1];
      }
      delete []top_diff_pair;
      delete []top_data_pair;
      delete []bottom_diff_pair;
  }
}

INSTANTIATE_CLASS(SoftmaxPairLayer);

}  // namespace caffe
