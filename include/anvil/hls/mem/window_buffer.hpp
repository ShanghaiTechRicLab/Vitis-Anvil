#pragma once

namespace anvil {
namespace hls {
namespace mem {

template <typename T, int Rows, int Cols>
struct window_buffer {
  static_assert(Rows > 0, "window_buffer: Rows must be positive");
  static_assert(Cols > 0, "window_buffer: Cols must be positive");

  typedef T value_type;
  static const int rows = Rows;
  static const int cols = Cols;

  T data[Rows][Cols];

  void partition_complete() {
#pragma HLS inline
#pragma HLS array_partition variable=data complete dim=0
  }

  T& at(int row, int col) {
#pragma HLS inline
    return data[row][col];
  }

  const T& at(int row, int col) const {
#pragma HLS inline
    return data[row][col];
  }

  template <int Row, int Col>
  T& at() {
#pragma HLS inline
    static_assert(Row >= 0 && Row < Rows, "window_buffer::at row out of range");
    static_assert(Col >= 0 && Col < Cols, "window_buffer::at col out of range");
    return data[Row][Col];
  }

  template <int Row, int Col>
  const T& at() const {
#pragma HLS inline
    static_assert(Row >= 0 && Row < Rows, "window_buffer::at row out of range");
    static_assert(Col >= 0 && Col < Cols, "window_buffer::at col out of range");
    return data[Row][Col];
  }

  template <int Row, int Col>
  void set(const T& value) {
#pragma HLS inline
    at<Row, Col>() = value;
  }

  void fill(const T& value) {
#pragma HLS inline
    for (int row = 0; row < Rows; ++row) {
      for (int col = 0; col < Cols; ++col) {
#pragma HLS unroll
        data[row][col] = value;
      }
    }
  }

  template <int Row, int Col>
  T get() const {
#pragma HLS inline
    static_assert(Row >= 0 && Row < Rows, "window_buffer::get row out of range");
    static_assert(Col >= 0 && Col < Cols, "window_buffer::get col out of range");
    return data[Row][Col];
  }

  void shift_left(int row, const T& value) {
#pragma HLS inline
    for (int col = 0; col < Cols - 1; ++col) {
#pragma HLS unroll
      data[row][col] = data[row][col + 1];
    }
    data[row][Cols - 1] = value;
  }

  template <int Row>
  void shift_left(const T& value) {
#pragma HLS inline
    static_assert(Row >= 0 && Row < Rows, "window_buffer::shift_left row out of range");
    for (int col = 0; col < Cols - 1; ++col) {
#pragma HLS unroll
      data[Row][col] = data[Row][col + 1];
    }
    data[Row][Cols - 1] = value;
  }

  void shift_up(int col, const T& value) {
#pragma HLS inline
    for (int row = 0; row < Rows - 1; ++row) {
#pragma HLS unroll
      data[row][col] = data[row + 1][col];
    }
    data[Rows - 1][col] = value;
  }

  template <int Col>
  void shift_up(const T& value) {
#pragma HLS inline
    static_assert(Col >= 0 && Col < Cols, "window_buffer::shift_up col out of range");
    for (int row = 0; row < Rows - 1; ++row) {
#pragma HLS unroll
      data[row][Col] = data[row + 1][Col];
    }
    data[Rows - 1][Col] = value;
  }
};

}  // namespace mem
}  // namespace hls
}  // namespace anvil
