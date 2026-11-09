/* Declaration-only GLES2 stub for desktop syntax checks. */
typedef unsigned int GLuint, GLenum, GLbitfield; typedef int GLint, GLsizei; typedef float GLfloat;
typedef unsigned char GLboolean; typedef long GLsizeiptr; typedef char GLchar;
enum { GL_VERTEX_SHADER=1, GL_FRAGMENT_SHADER, GL_COMPILE_STATUS, GL_LINK_STATUS, GL_TEXTURE_2D, GL_RGBA, GL_RGB,
 GL_UNSIGNED_BYTE, GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST,
 GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_REPEAT, GL_CLAMP_TO_EDGE, GL_UNSIGNED_SHORT_5_6_5, GL_UNSIGNED_SHORT_4_4_4_4,
 GL_UNSIGNED_SHORT_5_5_5_1, GL_UNPACK_ALIGNMENT, GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, GL_TEXTURE0,
 GL_DEPTH_TEST, GL_LEQUAL, GL_CULL_FACE, GL_BACK, GL_CCW, GL_BLEND, GL_FALSE=0, GL_TRUE=1, GL_SRC_ALPHA=100, GL_ONE,
 GL_ONE_MINUS_SRC_ALPHA, GL_FLOAT, GL_TRIANGLES, GL_UNSIGNED_SHORT, GL_TRIANGLE_STRIP, GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT };
GLuint glCreateProgram(void); GLuint glCreateShader(GLenum); void glShaderSource(GLuint,GLsizei,const GLchar*const*,const GLint*);
void glCompileShader(GLuint); void glGetShaderiv(GLuint,GLenum,GLint*); void glGetShaderInfoLog(GLuint,GLsizei,GLsizei*,GLchar*);
void glAttachShader(GLuint,GLuint); void glDeleteShader(GLuint); void glBindAttribLocation(GLuint,GLuint,const GLchar*);
void glLinkProgram(GLuint); void glGetProgramiv(GLuint,GLenum,GLint*); void glGetProgramInfoLog(GLuint,GLsizei,GLsizei*,GLchar*);
GLint glGetUniformLocation(GLuint,const GLchar*); void glGenTextures(GLsizei,GLuint*); void glBindTexture(GLenum,GLuint);
void glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*); void glTexParameteri(GLenum,GLenum,GLint);
void glPixelStorei(GLenum,GLint); void glGenerateMipmap(GLenum); void glGenBuffers(GLsizei,GLuint*); void glBindBuffer(GLenum,GLuint);
void glBufferData(GLenum,GLsizeiptr,const void*,GLenum); void glDeleteBuffers(GLsizei,const GLuint*); void glDeleteTextures(GLsizei,const GLuint*);
void glUseProgram(GLuint); void glUniform3fv(GLint,GLsizei,const GLfloat*); void glUniform2f(GLint,GLfloat,GLfloat); void glUniform1i(GLint,GLint);
void glUniform1f(GLint,GLfloat); void glUniform4f(GLint,GLfloat,GLfloat,GLfloat,GLfloat); void glActiveTexture(GLenum);
void glEnableVertexAttribArray(GLuint); void glDisableVertexAttribArray(GLuint); void glEnable(GLenum); void glDisable(GLenum);
void glDepthFunc(GLenum); void glCullFace(GLenum); void glFrontFace(GLenum); void glDepthMask(GLboolean); void glBlendFunc(GLenum,GLenum);
void glUniformMatrix4fv(GLint,GLsizei,GLboolean,const GLfloat*); void glVertexAttribPointer(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*);
void glDrawElements(GLenum,GLsizei,GLenum,const void*); void glDrawArrays(GLenum,GLint,GLsizei); void glViewport(GLint,GLint,GLsizei,GLsizei);
void glClearColor(GLfloat,GLfloat,GLfloat,GLfloat); void glClear(GLbitfield);
#ifndef GL_NO_ERROR
#define GL_NO_ERROR 0
#endif
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
static inline const unsigned char *glGetString(GLenum n) { (void)n; return (const unsigned char *)"stub"; }
static inline GLenum glGetError(void) { return 0; }
#define GL_TRIANGLE_FAN 0x0006
#define GL_SCISSOR_TEST 0x0C11
#define GL_VIEWPORT 0x0BA2
void glScissor(GLint,GLint,GLsizei,GLsizei); void glGetIntegerv(GLenum,GLint*); void glVertexAttrib4f(GLuint,GLfloat,GLfloat,GLfloat,GLfloat);
void glGenVertexArraysOES(GLsizei,GLuint*); void glBindVertexArrayOES(GLuint); void glDeleteVertexArraysOES(GLsizei,const GLuint*);
