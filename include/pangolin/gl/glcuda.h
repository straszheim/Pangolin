/* This file is part of the Pangolin Project.
 * http://github.com/stevenlovegrove/Pangolin
 *
 * Copyright (c) 2011 Steven Lovegrove
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <algorithm>
#include <system_error>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

#include "gl.h"

namespace pangolin
{

  void throw_cuda_error(cudaError_t err, const char* prettyfunction_, const char* file_,
                        int line_);

  class CudaError : std::system_error
  {
  public:
  CudaError(cudaError_t err_)
    : errcode(err_)
    { }

    const char* what() {
      return cudaGetErrorString(errcode);
    }

    cudaError_t errcode;
  };

  void throw_cuda_error(cudaError_t err, const char* prettyfunction_, const char* file_,
                        int line_) {
    std::cerr << "***\n*** " << prettyfunction_ << " " << file_ << ":" << line_
              << " Cuda badness!  Error code " << err << " aka \""
              << cudaGetErrorString(err) << "\"\n***\n";
    throw CudaError(err);
  }


  struct CudaCheck {
    const char* prettyfunction_;
    const char* file_;
    int line_;

  CudaCheck(const char* prettyfunction, const char* file, int line)
  : prettyfunction_(prettyfunction)
  , file_(file)
      , line_(line)
    { }

    void operator=(cudaError_t err) const {
      if (err == cudaSuccess)
        return;
      throw_cuda_error(err, prettyfunction_, file_, line_);
    }
  };

#define PG_CUDACHECK ::pangolin::CudaCheck(__PRETTY_FUNCTION__,__FILE__, __LINE__)

  ////////////////////////////////////////////////
  // Interface
  ////////////////////////////////////////////////

  struct GlBufferCudaPtr : public GlBuffer
  {
    friend class CudaScopedMappedPtr;
    //! Default constructor represents 'no buffer'
    GlBufferCudaPtr();

    GlBufferCudaPtr(GlBufferType buffer_type, GLuint size_bytes, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ );
    GlBufferCudaPtr(GlBufferType buffer_type, GLuint num_elements, GLenum datatype, GLuint count_per_element, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ );

    PANGOLIN_DEPRECATED
      GlBufferCudaPtr(GlBufferType buffer_type, GLuint width, GLuint height, GLenum datatype, GLuint count_per_element, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ );

    ~GlBufferCudaPtr();

    void Reinitialise(GlBufferType buffer_type, GLuint size_bytes, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ );
    void Reinitialise(GlBufferType buffer_type, GLuint num_elements, GLenum datatype, GLuint count_per_element, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ );

    /**
     * Use parameters from another @c GlBufferCudaPtr to initialize this buffer.
     */
    void Reinitialise(const GlBufferCudaPtr& other);

    unsigned int cuda_use;
    cudaGraphicsResource* cuda_res;

  private:
    void map() const {
      PG_CUDACHECK = cudaGraphicsMapResources(1, const_cast<cudaGraphicsResource**>(&cuda_res), 0);
      mapped_ = true;
    }

    bool mapped() const { return mapped_; }

    void unmap() const {
      PG_CUDACHECK = cudaGraphicsUnmapResources(1, const_cast<cudaGraphicsResource**>(&cuda_res), 0);
      mapped_ = false;
    }

    mutable bool mapped_;
  };

  struct GlTextureCudaArray : GlTexture
  {
    GlTextureCudaArray();
    // Some internal_formats aren't accepted. I have trouble with GL_RGB8
    GlTextureCudaArray(int width, int height, GLint internal_format, bool sampling_linear = true, int border = 0, GLenum glformat = GL_RGBA, GLenum gltype = GL_UNSIGNED_BYTE, GLvoid* data = NULL);
    ~GlTextureCudaArray();

    void Reinitialise(int width, int height, GLint internal_format, bool sampling_linear = true, int border = 0, GLenum glformat = GL_RGBA, GLenum gltype = GL_UNSIGNED_BYTE, GLvoid* data = NULL) override;
    cudaGraphicsResource* cuda_res;
  };

  struct CudaScopedMappedPtr
  {
    CudaScopedMappedPtr(const GlBufferCudaPtr& buffer);
    ~CudaScopedMappedPtr();
    void* operator*() const;
  private:
    CudaScopedMappedPtr(const CudaScopedMappedPtr&) = delete;
    const GlBufferCudaPtr& buf;
  };

  struct CudaScopedMappedArray
  {
    CudaScopedMappedArray(const GlTextureCudaArray& tex);
    ~CudaScopedMappedArray();
    cudaArray* operator*();
    cudaGraphicsResource* res;

  private:
    CudaScopedMappedArray(const CudaScopedMappedArray&) {}
  };

  void CopyPboToTex(GlBufferCudaPtr& buffer, GlTexture& tex);

  void swap(GlBufferCudaPtr& a, GlBufferCudaPtr& b);

  ////////////////////////////////////////////////
  // Implementation
  ////////////////////////////////////////////////

  inline GlBufferCudaPtr::GlBufferCudaPtr()
    : cuda_use(0), cuda_res(0), mapped_(false)
  {
  }

  inline GlBufferCudaPtr::GlBufferCudaPtr(GlBufferType buffer_type, GLuint size_bytes, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ )
    : GlBufferCudaPtr()
  {
    Reinitialise(buffer_type, size_bytes, cudause, gluse);
  }

  inline GlBufferCudaPtr::GlBufferCudaPtr(GlBufferType buffer_type, GLuint num_elements, GLenum datatype, GLuint count_per_element, unsigned int cudause, GLenum gluse )
    : GlBufferCudaPtr()
  {
    Reinitialise(buffer_type, num_elements, datatype, count_per_element, cudause, gluse);
  }

  inline GlBufferCudaPtr::GlBufferCudaPtr(GlBufferType buffer_type, GLuint width, GLuint height, GLenum datatype, GLuint count_per_element, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ )
    : GlBufferCudaPtr()
  {
    Reinitialise(buffer_type, width*height, datatype, count_per_element, cudause, gluse);
  }

  inline GlBufferCudaPtr::~GlBufferCudaPtr()
  {
    if(cuda_res) {
      PG_CUDACHECK = cudaGraphicsUnregisterResource(cuda_res);
    }
  }

  inline void GlBufferCudaPtr::Reinitialise(GlBufferType buffer_type, GLuint size_bytes, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ )
  {
    GlBufferCudaPtr::Reinitialise(buffer_type, size_bytes, GL_BYTE, 1, cudause, gluse);
  }

  inline void GlBufferCudaPtr::Reinitialise(GlBufferType buffer_type, GLuint num_elements, GLenum datatype, GLuint count_per_element, unsigned int cudause /*= cudaGraphicsMapFlagsNone*/, GLenum gluse /*= GL_DYNAMIC_DRAW*/ )
  {
    if(cuda_res) {
      cudaGraphicsUnregisterResource(cuda_res);
    }
    GlBuffer::Reinitialise(buffer_type, num_elements, datatype, count_per_element, gluse);

    cuda_use = cudause;
    cudaGraphicsGLRegisterBuffer( &cuda_res, bo, cudause );
  }

  inline void GlBufferCudaPtr::Reinitialise(const GlBufferCudaPtr& other)
  {
    Reinitialise(other.buffer_type, other.num_elements, other.datatype, other.count_per_element, other.cuda_use, other.gluse);
  }

  inline GlTextureCudaArray::GlTextureCudaArray()
    : GlTexture(), cuda_res(0)
    {
      // Not a texture
    }

  inline GlTextureCudaArray::GlTextureCudaArray(int width, int height, GLint internal_format, bool sampling_linear, int border, GLenum glformat, GLenum gltype, GLvoid *data)
    :GlTexture(width,height,internal_format, sampling_linear, border, glformat, gltype, data)
  {
    // TODO: specify flags too
    const cudaError_t err = cudaGraphicsGLRegisterImage(&cuda_res, tid, GL_TEXTURE_2D, cudaGraphicsMapFlagsNone);
    if( err != cudaSuccess ) {
      std::cout << "cudaGraphicsGLRegisterImage failed: " << err << std::endl;
    }
  }

  inline GlTextureCudaArray::~GlTextureCudaArray()
  {
    if(cuda_res) {
      PG_CUDACHECK = cudaGraphicsUnregisterResource(cuda_res);
    }
  }

  inline void GlTextureCudaArray::Reinitialise(int width, int height, GLint internal_format, bool sampling_linear, int border, GLenum glformat, GLenum gltype, GLvoid* data)
  {
    if(cuda_res) {
      cudaGraphicsUnregisterResource(cuda_res);
    }

    GlTexture::Reinitialise(width, height, internal_format, sampling_linear, border, glformat, gltype, data);

    const cudaError_t err = cudaGraphicsGLRegisterImage(&cuda_res, tid, GL_TEXTURE_2D, cudaGraphicsMapFlagsNone);
    if( err != cudaSuccess ) {
      std::cout << "cudaGraphicsGLRegisterImage failed: " << err << std::endl;
    }
  }

  inline CudaScopedMappedPtr::CudaScopedMappedPtr(const GlBufferCudaPtr& buffer)
    : buf(buffer)
  {
    assert(!buf.mapped());
    buf.map();
  }

  inline CudaScopedMappedPtr::~CudaScopedMappedPtr()
  {
    assert(buf.mapped());
    buf.unmap();
  }

  inline void* CudaScopedMappedPtr::operator*() const
  {
    size_t num_bytes;
    void* d_ptr;
    assert(buf.mapped());
    PG_CUDACHECK = cudaGraphicsResourceGetMappedPointer(&d_ptr,&num_bytes,buf.cuda_res);
    return d_ptr;
  }

  inline CudaScopedMappedArray::CudaScopedMappedArray(const GlTextureCudaArray& tex)
    : res(tex.cuda_res)
  {
    PG_CUDACHECK = cudaGraphicsMapResources(1, &res);
  }

  inline CudaScopedMappedArray::~CudaScopedMappedArray()
  {
    PG_CUDACHECK = cudaGraphicsUnmapResources(1, &res);
  }

  inline cudaArray* CudaScopedMappedArray::operator*()
  {
    cudaArray* array;
    cudaGraphicsSubResourceGetMappedArray(&array, res, 0, 0);
    return array;
  }

  inline void CopyPboToTex(const GlBufferCudaPtr& buffer, GlTexture& tex, GLenum buffer_layout, GLenum buffer_data_type )
  {
    buffer.Bind();
    tex.Bind();
    glTexImage2D(GL_TEXTURE_2D, 0, tex.internal_format, tex.width, tex.height, 0, buffer_layout, buffer_data_type, 0);
    buffer.Unbind();
    tex.Unbind();
  }

  template<typename T>
    inline void CopyDevMemtoTex(T* d_img, size_t pitch, GlTextureCudaArray& tex )
    {
      CudaScopedMappedArray arr_tex(tex);
      PG_CUDACHECK = cudaMemcpy2DToArray(*arr_tex, 0, 0, d_img, pitch, tex.width*sizeof(T), tex.height, cudaMemcpyDeviceToDevice );
    }

  inline void swap(GlBufferCudaPtr& a, GlBufferCudaPtr& b)
  {
    std::swap(a.bo, b.bo);
    std::swap(a.cuda_res, b.cuda_res);
    std::swap(a.buffer_type, b.buffer_type);
  }


}
