/* RUN: %{execute}%s
 */
#include <cstdlib>

#include "include/jacobi-stencil.hpp"

inline float& fdl_out(int a,int b, cl::sycl::accessor<float, 2, cl::sycl::access::mode::write> acc) {return acc[a][b];}
inline float  fdl_in(int a,int b, cl::sycl::accessor<float, 2, cl::sycl::access::mode::read>  acc) {return acc[a][b];}

// static declaration to use pointers
cl::sycl::buffer<float,2> ioBuffer;
cl::sycl::buffer<float,2> ioABuffer;

int main(int argc, char **argv) {
  read_args(argc, argv);
  struct counters timer;
  start_measure(timer);

  // Assign new buffers to the global buffers
  ioBuffer = cl::sycl::buffer<float,2>(cl::sycl::range<2> {M, N});
  ioABuffer = cl::sycl::buffer<float,2>(cl::sycl::range<2> {M, N});
#if DEBUG_STENCIL
  std::vector<float> a_test(M * N);
  std::vector<float> b_test(M * N);
#endif

  // initialization
  for (size_t i = 0; i < M; ++i){
    for (size_t j = 0; j < N; ++j){
      float value = ((float) i*(j+2) + 10) / N;
      cl::sycl::id<2> id = {i, j};
      ioBuffer.get_access<cl::sycl::access::mode::write, cl::sycl::access::target::host_buffer>()[id] = value;
      ioABuffer.get_access<cl::sycl::access::mode::write, cl::sycl::access::target::host_buffer>()[id] = value;
#if DEBUG_STENCIL
      a_test[i*N+j] = value;
      b_test[i*N+j] = value;
#endif
    }
  }

  // our work
  coef_fxd2D<0,0> c_id {1.0f};
  coef_fxd2D<0, 0> c1 {MULT_COEF};  
  coef_fxd2D<1, 0> c2 {MULT_COEF};
  coef_fxd2D<0, 1> c3 {MULT_COEF};
  coef_fxd2D<-1, 0> c4 {MULT_COEF};
  coef_fxd2D<0, -1> c5 {MULT_COEF};

  auto st = c1+c2+c3+c4+c5;
  input_fxd2D<float, &ioABuffer, &fdl_in> work_in;
  output_2D<float, &ioBuffer, &fdl_out> work_out;
  auto op_work = work_out << st << work_in;

  auto st_id = c_id.toStencil();
  input_fxd2D<float, &ioBuffer, &fdl_in> copy_in;
  output_2D<float, &ioABuffer, &fdl_out> copy_out;
  auto op_copy = copy_out << st_id << copy_in;

  end_init(timer);

  auto begin_op = counters::clock_type::now();

  // compute result with "gpu"
  {   
    cl::sycl::queue myQueue; 
    for (unsigned int i = 0; i < NB_ITER; ++i){      
      //op_work.doComputation(myQueue);
      op_work.doLocalComputation(myQueue);
      op_copy.doComputation(myQueue);
    }
  }

  auto end_op = counters::clock_type::now();
  timer.stencil_time = std::chrono::duration_cast<counters::duration_type>(end_op - begin_op);
  // loading time is not watched
  end_measure(timer);

#if DEBUG_STENCIL
  // get the gpu result
  auto C = (ioABuffer).get_access<cl::sycl::access::mode::read, cl::sycl::access::target::host_buffer>();
  ute_and_are(a_test,b_test,C);
#endif

  return 0;
}
