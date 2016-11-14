#include <vector>

#include "caffe/layers/conv_layer.hpp"

namespace caffe {

template <typename Dtype>
void ConvolutionLayer<Dtype>::compute_output_shape() {
  const int* kernel_shape_data = this->kernel_shape_.cpu_data();
  const int* stride_data = this->stride_.cpu_data();
  const int* pad_data = this->pad_.cpu_data();
  const int* dilation_data = this->dilation_.cpu_data();
  this->output_shape_.clear();
  for (int i = 0; i < this->num_spatial_axes_; ++i) {
    // i + 1 to skip channel axis
    const int input_dim = this->input_shape(i + 1);
    const int kernel_extent = dilation_data[i] * (kernel_shape_data[i] - 1) + 1;
    const int output_dim = (input_dim + 2 * pad_data[i] - kernel_extent)
        / stride_data[i] + 1;
    this->output_shape_.push_back(output_dim);
  }
}

template <typename Dtype>
void ConvolutionLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  const Dtype* weight = this->blobs_[0]->cpu_data();
  for (int i = 0; i < bottom.size(); ++i) {
    const Dtype* bottom_data = bottom[i]->cpu_data();
    Dtype* top_data = top[i]->mutable_cpu_data();
    for (int n = 0; n < this->num_; ++n) { // JSP: this->num_ is batch size
      this->forward_cpu_gemm(bottom_data + n * this->bottom_dim_, weight,
          top_data + n * this->top_dim_, n);
      if (this->bias_term_) {
        const Dtype* bias = this->blobs_[1]->cpu_data();
        this->forward_cpu_bias(top_data + n * this->top_dim_, bias);
      }
    }
  }
}

template <typename Dtype>
void ConvolutionLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  const Dtype* weight = this->blobs_[0]->cpu_data();
  Dtype* weight_diff = this->blobs_[0]->mutable_cpu_diff();
  for (int i = 0; i < top.size(); ++i) {
    const Dtype* top_diff = top[i]->cpu_diff();
    const Dtype* bottom_data = bottom[i]->cpu_data();
    Dtype* bottom_diff = bottom[i]->mutable_cpu_diff();
    // Bias gradient, if necessary.
    if (this->bias_term_ && this->param_propagate_down_[1]) {
      Dtype* bias_diff = this->blobs_[1]->mutable_cpu_diff();
      for (int n = 0; n < this->num_; ++n) {
        this->backward_cpu_bias(bias_diff, top_diff + n * this->top_dim_);
      }
    }
    if (this->param_propagate_down_[0] || propagate_down[i]) {
      for (int n = 0; n < this->num_; ++n) {
        // gradient w.r.t. weight. Note that we will accumulate diffs.
        if (this->param_propagate_down_[0]) {
          for (int i = 0; i < this->bottom_dim_; ++i) {
            if (isnan(bottom_data[n*this->bottom_dim_ + i])) {
              ostringstream str;
              str << "nan at bottom_data[" << n << ", " << i << "]";
              LOG(FATAL) << str.str();
            }
          }
          for (int i = 0; i < this->top_dim_; ++i) {
            if (isnan(top_diff[n*this->top_dim_ + i])) {
              ostringstream str;
              str << "nan at top_diff[" << n << ", " << i << "]";
              LOG(FATAL) << str.str();
            }
          }

          this->weight_cpu_gemm(bottom_data + n * this->bottom_dim_,
              top_diff + n * this->top_dim_, weight_diff);
          for (int i = 0; i < this->blobs_[0]->count(); ++i) {
            if (isnan(weight_diff[i])) {
              ostringstream str;
              str << "nan at weight_diff[" << i << "]";
              LOG(FATAL) << str.str();
            }
          }

          if (this->layer_param().name() == "conv1") {
            fprintf(stderr, "weight_diff[%d]\n", n);
            for (int m = 0; m < this->conv_out_channels_; ++m) {
              for (int c = 0; c < this->conv_in_channels_/this->group_; ++c) {
                for (int i = 0; i < kernel_h*kernel_w; ++i) {
                  fprintf(stderr, "%g ", weight_diff[(m*(this->conv_in_channels_/this->group_) + c)*kernel_h*kernel_w + i]);
                }
              }
              fprintf(stderr, "\n");
            }
          }
        }
        // gradient w.r.t. bottom data, if necessary.
        if (propagate_down[i]) {
          for (int i = 0; i < this->blobs_[0]->count(); ++i) {
            if (isnan(weight[i])) {
              ostringstream str;
              str << "nan at weight[" << i << "]";
              LOG(FATAL) << str.str();
            }
          }
          this->backward_cpu_gemm(top_diff + n * this->top_dim_, weight,
              bottom_diff + n * this->bottom_dim_);
          for (int i = 0; i < this->bottom_dim_; ++i) {
            if (isnan(bottom_diff[n*this->bottom_dim_ + i])) {
              ostringstream str;
              str << "nan at bottom_diff[" << n << ", " << i << "]";
              LOG(FATAL) << str.str();
            }
          }
        }
      }
    }
  }
}

#ifdef CPU_ONLY
STUB_GPU(ConvolutionLayer);
#endif

INSTANTIATE_CLASS(ConvolutionLayer);

}  // namespace caffe
