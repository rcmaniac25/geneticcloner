// Default empty project template
#include "GeneticCloner.h"

#include <bb/cascades/Application>
#include <bb/cascades/Window>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/ImageView>
#include <bb/cascades/ForeignWindowControl>

#include <bb/cascades/Image>

#include <QDir>

#include <math.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

using namespace bb::cascades;

#ifndef GL_GLEXT_PROTOTYPES
#ifdef GL_IMG_multisampled_render_to_texture
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC glRenderbufferStorageMultisampleIMG = NULL;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC glFramebufferTexture2DMultisampleIMG = NULL;
#endif
#ifdef GL_EXT_discard_framebuffer
PFNGLDISCARDFRAMEBUFFEREXTPROC glDiscardFramebufferEXT = NULL;
#endif
#ifdef GL_OES_vertex_array_object
PFNGLBINDVERTEXARRAYOESPROC glBindVertexArrayOES = NULL;
PFNGLGENVERTEXARRAYSOESPROC glGenVertexArraysOES = NULL;
#endif
#endif

GeneticCloner::GeneticCloner(bb::cascades::Application* app) : QObject(app),
		initialized(false), totalMutations(0), totalImprovements(0), totalData(0), lowestDifference(0), circles(), glWindowSize(), tmpData(NULL), imagePath(),
		originalImage(NULL), timer(NULL), image(NULL),
		context(NULL), window(NULL), eglDisp(EGL_NO_DISPLAY), eglSurf(EGL_NO_SURFACE), eglCtx(EGL_NO_CONTEXT), eglConf(NULL),
		frameBuffer(0), renderbuffer(0), drawTexture(0), circleProgram(0), textureProgram(0), circleSegCount(0), circleVAO(0), textureVAO(0)
{
    QmlDocument* qml = QmlDocument::create("asset:///main.qml").parent(this).property("cloner", this);

    timer = new QTimer(this);
    timer->setInterval(0);
    bool ret = QObject::connect(timer, SIGNAL(timeout()), this, SLOT(updateImage()));
    Q_ASSERT(ret);
    Q_UNUSED(ret)

    app->setMenuEnabled(false);

    AbstractPane* root = qml->createRootObject<AbstractPane>();

    originalImage = root->findChild<ImageView*>("originalImageView");
    ForeignWindowControl* fwc = root->findChild<ForeignWindowControl*>("mutatedFWC");
    glWindowSize = QSize((int)fwc->preferredWidth(), (int)fwc->preferredHeight());

    tmpData = new uchar[glWindowSize.width() * glWindowSize.height() * 4];

    app->setScene(root);
}

GeneticCloner::~GeneticCloner()
{
	timer->stop();
	delete image;
	delete[] tmpData;
	if (eglDisp != EGL_NO_DISPLAY)
	{
		eglMakeCurrent(eglDisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (eglSurf != EGL_NO_SURFACE)
		{
			eglDestroySurface(eglDisp, eglSurf);
			eglSurf = EGL_NO_SURFACE;
		}
		if(window)
		{
			screen_destroy_window(window);
			window = NULL;
		}
		if (eglCtx != EGL_NO_CONTEXT)
		{
			eglDestroyContext(eglDisp, eglCtx);
			eglCtx = EGL_NO_CONTEXT;
		}
		eglTerminate(eglDisp);
		eglDisp = EGL_NO_DISPLAY;
	}
	eglReleaseThread();
}

float GeneticCloner::fitness() const
{
	if(totalData == 0)
	{
		return 0.0f;
	}
	long long value = (long long)((1.0f - (float)lowestDifference / (float)totalData) * 10000.0f);
	return value * (1.0f / 100.0f);
}

long long GeneticCloner::improvements() const
{
	return totalImprovements;
}

long long GeneticCloner::mutations() const
{
	return totalMutations;
}

void GeneticCloner::setImage(const QString& file)
{
	if(imagePath == file)
	{
		//Same image, don't change anything
		return;
	}
	imagePath = file;
	if(image)
	{
		delete image;
		image = NULL;
	}

	bool running = timer->isActive();
	timer->stop();
	initialized = false;

	QDir dir("./app/native/assets/Images");

	image = new QImage();
	if(image->load(dir.filePath(file)))
	{
		QImage swapped = image->rgbSwapped();
		QSize swappedSize = swapped.size();

		glGenTextures(1, (GLuint*)&tempTexture);
		glBindTexture(GL_TEXTURE_2D, tempTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, swappedSize.width(), swappedSize.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, swapped.constBits());
		glBindTexture(GL_TEXTURE_2D, 0);

		originalImage->setImage(Image(bb::ImageData::fromPixels(swapped.constBits(), bb::PixelFormat::RGBX, swappedSize.width(), swappedSize.height(), swapped.bytesPerLine())));
	}

	if(running)
	{
		toggleRunning();
	}
	else
	{
		//toggleRunning will reinit and clear the window, but if toggleRunning is not called, we have to clear it ourselves

		eglMakeCurrent(eglDisp, eglSurf, eglSurf, eglCtx);

		clearMutationWindow();

		eglSwapBuffers(eglDisp, eglSurf);
	}
}

void GeneticCloner::drawFramebuffer()
{
	glUseProgram(textureProgram);

	glBindVertexArrayOES(textureVAO);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, drawTexture);
	glUniform1i(glGetUniformLocation(textureProgram, "un_texture"), 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindTexture(GL_TEXTURE_2D, 0);

	glBindVertexArrayOES(0);

	glUseProgram(0);
}

void GeneticCloner::clearMutationWindow()
{
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef GL_EXT_discard_framebuffer
	if(strstr((char*)glGetString(GL_EXTENSIONS), "GL_EXT_discard_framebuffer"))
	{
		GLenum discard_attachments[] = { GL_DEPTH_EXT };
		glDiscardFramebufferEXT(GL_FRAMEBUFFER, 1, discard_attachments);
	}
#endif

	drawFramebuffer();
}

long long totalDifference(const QImage* sourceImage, int framebuffer, uchar* data)
{
	QImage swapped = sourceImage->rgbSwapped();
	const uchar* swappedData = swapped.constBits();
	QSize swappedSize = swapped.size();

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glReadPixels(0, 0, swappedSize.width(), swappedSize.height(), GL_RGBA, GL_UNSIGNED_BYTE, data);

	long long total = 0;
	int dataWidth = swappedSize.width() * 4;
	int dataWidthDiff = swapped.bytesPerLine() - dataWidth;
	int maxSize = swappedSize.width() * swappedSize.height() * 4;
	for(int s = 0, b = 0; b < maxSize; s += 4, b += 4)
	{
		if(dataWidthDiff != 0 && b != 0 && (b % dataWidth) == 0)
		{
			s += dataWidthDiff;
		}
		total += (long long)abs(swappedData[s] - data[b]) + (long long)abs(swappedData[s+1] - data[b+1]) + (long long)abs(swappedData[s+2] - data[b+2]);
	}

	return total;
}

void GeneticCloner::toggleRunning()
{
	if(!initialized)
	{
		totalMutations = 0;
		totalImprovements = 0;
		totalData = 0;
		lowestDifference = 0;

		emit fitnessChanged(0.0f);
		emit improvementsChanged(0);
		emit mutationsChanged(0);

		//Reset the draw area
		eglMakeCurrent(eglDisp, eglSurf, eglSurf, eglCtx);

		clearMutationWindow();

		eglSwapBuffers(eglDisp, eglSurf);

		// will initialize as the total of all image data in `source`
		//   because `best` is empty
		totalData = totalDifference(image, frameBuffer, tmpData);

		// the current 'best' starts off as the total
		lowestDifference = totalData;

		//Reset circles
		circles.clear();

		initialized = true;
	}

	if(timer->isActive())
	{
		timer->stop();
	}
	else
	{
		timer->start();
	}

	updateImage();
}

QString readFileEntireText(const QString& name)
{
	QFile file(name);

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return QString();
	}

	return QString(file.readAll());
}

GLuint compileProgram(const QString& vertShader, const QString& fragShader)
{
	GLuint vshaderObject = 0, fshaderObject = 0, program = 0;
	GLint length, status;

	//Setup the fragment shader
	QString shader = readFileEntireText(fragShader);
	if(shader.isNull())
	{
		qDebug() << "Fragment shader is NULL, exiting";
		return 0;
	}
	fshaderObject = glCreateShader(GL_FRAGMENT_SHADER);

	QByteArray dataArray = shader.toAscii();
	int dataSize = dataArray.length();
	const char* data = dataArray.constData();
	glShaderSource(fshaderObject, 1, (const GLchar* const*)&data, &dataSize);
	glCompileShader(fshaderObject);

	//Check for errors
	glGetShaderiv(fshaderObject, GL_COMPILE_STATUS, &status);
	if (!status)
	{
		glGetShaderiv(fshaderObject, GL_INFO_LOG_LENGTH, &length);

		GLchar* log = new GLchar[length + 1];

		glGetShaderInfoLog(fshaderObject, length, &length, log);
		qDebug() << "Fragment shader compile log = '" + QString(log) + "'";

		delete[] log;

		glDeleteShader(fshaderObject);
		return 0;
	}

	//Setup program
	program = glCreateProgram();
	glAttachShader(program, fshaderObject);

	//Setup the vertices shader
	shader = readFileEntireText(vertShader);
	if(shader.isNull())
	{
		qDebug() << "Vertex shader is NULL, exiting";
		glDeleteProgram(program);
		glDeleteShader(fshaderObject);
		return 0;
	}
	vshaderObject = glCreateShader(GL_VERTEX_SHADER);

	dataArray = shader.toAscii();
	dataSize = dataArray.length();
	data = dataArray.constData();
	glShaderSource(vshaderObject, 1, (const GLchar* const*)&data, &dataSize);
	glCompileShader(vshaderObject);

	//Check for errors
	glGetShaderiv(vshaderObject, GL_COMPILE_STATUS, &status);
	if (!status)
	{
		glGetShaderiv(vshaderObject, GL_INFO_LOG_LENGTH, &length);

		GLchar* log = new GLchar[length + 1];

		glGetShaderInfoLog(vshaderObject, length, &length, log);
		qDebug() << "Vertex shader compile log = '" + QString(log) + "'";

		delete[] log;

		glDeleteProgram(program);
		glDeleteShader(fshaderObject);
		glDeleteShader(vshaderObject);
		return 0;
	}

	glAttachShader(program, vshaderObject);

	//Link program
	glLinkProgram(program);

	//Check for errors
	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if (!status)
	{
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

		GLchar* log = new GLchar[length + 1];

		glGetProgramInfoLog(program, length, &length, log);
		qDebug() << "Program link log = '" + QString(log) + "'";

		delete[] log;
		glDeleteProgram(program);
		glDeleteShader(fshaderObject);
		glDeleteShader(vshaderObject);
		return 0;
	}

#ifdef DEBUG
	//Validate program
	glValidateProgram(program);

	glGetProgramiv(program, GL_VALIDATE_STATUS, &status);

	if (!status)
	{
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

		GLchar* log = new GLchar[length + 1];

		glGetProgramInfoLog(program, length, &length, log);
		qDebug() << "Program link log = '" + QString(log) + "'";

		delete[] log;
		glDeleteProgram(program);
		glDeleteShader(fshaderObject);
		glDeleteShader(vshaderObject);
		return 0;
	}
#endif

	return program;
}

int generateCircle(float radius, GLint vertexAtt, int* segCount)
{
	//Based off http://slabode.exofire.net/circle_draw.shtml
	//Use GL_LINE_LOOP to draw

	(*segCount) = 10 * sqrtf(radius);

	float theta = 2 * 3.1415926 / float(*segCount);
	float c = cosf(theta); //precalculate the sine and cosine
	float s = sinf(theta);
	float t;

	float x = radius;//we start at angle = 0
	float y = 0;

	GLsizeiptr circleSize = (*segCount) * 2 * sizeof(GLfloat);
	GLfloat* circle = new GLfloat[(*segCount) * 2];

	for(int ii = 0; ii < (*segCount); ii++)
	{
		int pos = ii * 2;

		//output vertex
		circle[pos] = x;
		circle[pos + 1] = y;

		//apply the rotation matrix
		t = x;
		x = c * x - s * y;
		y = s * t + c * y;
	}

	GLuint vao = 0;
	GLuint vbo;
	glGenVertexArraysOES(1, &vao);
	glBindVertexArrayOES(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, circleSize, circle, GL_STATIC_DRAW);
	glVertexAttribPointer(vertexAtt, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(vertexAtt);
	glBindVertexArrayOES(0);

	delete[] circle;

	return vao;
}

GLfloat* pretentGlOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GLfloat* matrix = new GLfloat[4 * 4];

	memset(matrix, 0, sizeof(matrix));

	matrix[0] = 2 / (right - left);								//X scale
	matrix[12] = -((right + left) / (right - left));			//X trans

	matrix[5] = 2 / (top - bottom);								//Y scale
	matrix[13] = -((top + bottom) / (top - bottom));			//Y trans

	matrix[10] = -2 / (farVal - nearVal);						//Z scale
	matrix[14] = -((farVal + nearVal) / (farVal - nearVal));	//Z trans

	matrix[15] = 1;												//W scale

	return matrix;
}

void GeneticCloner::init(bb::cascades::Application* app)
{
	ForeignWindowControl* fwc = app->scene()->findChild<ForeignWindowControl*>("mutatedFWC");

	fwc->setWindowId("GeneticClonerFWCId");
	fwc->setWindowGroup(app->mainWindow()->groupId());

	//Setup OpenGL ES 2.0
	EGLint attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	EGLint attrib_list[] = {EGL_RED_SIZE,        8,
							EGL_GREEN_SIZE,      8,
							EGL_BLUE_SIZE,       8,
							EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
							EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
							EGL_NONE};
	int usage = SCREEN_USAGE_OPENGL_ES2;
	int screenFormat = SCREEN_FORMAT_RGBX8888;
	int z = 0x80000000; //Really small number
	int w, h;
	int bufferSize[] = {w = glWindowSize.width(), h = glWindowSize.height()};

	//Setup OpenGL
	eglDisp = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	eglInitialize(eglDisp, NULL, NULL);

	eglBindAPI(EGL_OPENGL_ES_API);

	EGLint numConfigs;
	eglChooseConfig(eglDisp, attrib_list, &eglConf, 1, &numConfigs);

	eglCtx = eglCreateContext(eglDisp, eglConf, EGL_NO_CONTEXT, attributes);

	//Setup Window
	screen_create_context(&context, SCREEN_APPLICATION_CONTEXT);
	screen_create_window_type(&window, context, SCREEN_CHILD_WINDOW);
	screen_set_window_property_iv(window, SCREEN_PROPERTY_USAGE, &usage);
	screen_set_window_property_iv(window, SCREEN_PROPERTY_FORMAT, &screenFormat);
	screen_set_window_property_iv(window, SCREEN_PROPERTY_ZORDER, &z);
	screen_set_window_property_iv(window, SCREEN_PROPERTY_BUFFER_SIZE, bufferSize);
	screen_create_window_buffers(window, 2);

	//Setup the window groups
	QByteArray groupArr = fwc->windowGroup().toAscii();
	QByteArray idArr = fwc->windowId().toAscii();

	//Setup group and ID
	screen_join_window_group(window, groupArr.constData());
	screen_set_window_property_cv(window, SCREEN_PROPERTY_ID_STRING, idArr.length(), idArr.constData());

	//Setup FWC
	fwc->setWindowHandle(window);

	//Set the remaining components so OpenGL operations can be performed
	eglSurf = eglCreateWindowSurface(eglDisp, eglConf, window, NULL);

	eglMakeCurrent(eglDisp, eglSurf, eglSurf, eglCtx);

	eglSwapInterval(eglDisp, 1);

	const char* extGL = (const char*)glGetString(GL_EXTENSIONS);
#if defined(GL_IMG_multisampled_render_to_texture)
#define SAMPLES 0 //XXX For now
#if !defined(GL_GLEXT_PROTOTYPES)
	if(strstr((char*)extGL, "GL_IMG_multisampled_render_to_texture"))
	{
		glRenderbufferStorageMultisampleIMG = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC)eglGetProcAddress("glRenderbufferStorageMultisampleIMG");
		glFramebufferTexture2DMultisampleIMG = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleIMG");
	}
#endif
#endif
#if defined(GL_EXT_discard_framebuffer) && !defined(GL_GLEXT_PROTOTYPES)
	if(strstr((char*)extGL, "GL_EXT_discard_framebuffer"))
	{
		glDiscardFramebufferEXT = (PFNGLDISCARDFRAMEBUFFEREXTPROC)eglGetProcAddress("glDiscardFramebufferEXT");
	}
#endif
#if defined(GL_OES_vertex_array_object) && !defined(GL_GLEXT_PROTOTYPES)
	if(strstr((char*)extGL, "GL_OES_vertex_array_object"))
	{
		glBindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC)eglGetProcAddress("glBindVertexArrayOES");
		glGenVertexArraysOES = (PFNGLGENVERTEXARRAYSOESPROC)eglGetProcAddress("glGenVertexArraysOES");
	}
	Q_ASSERT(glBindVertexArrayOES);
	Q_ASSERT(glGenVertexArraysOES);
#endif

	//Setup programs
	circleProgram = compileProgram("./app/native/assets/circle.vert", "./app/native/assets/circle.frag");
	textureProgram = compileProgram("./app/native/assets/texture.vert", "./app/native/assets/texture.frag");

	GLfloat* projectionMatrix = pretentGlOrtho(0, w, 0, h, 0, 1);
	glUseProgram(circleProgram);
	glUniformMatrix4fv(glGetUniformLocation(circleProgram, "un_ProjectionMatrix"), 1, GL_FALSE, projectionMatrix);
	glUseProgram(textureProgram);
	glUniformMatrix4fv(glGetUniformLocation(textureProgram, "un_ProjectionMatrix"), 1, GL_FALSE, projectionMatrix);
	glUseProgram(0);
	delete[] projectionMatrix;

	//Setup buffers
	glGenRenderbuffers(1, (GLuint*)&renderbuffer);

	glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
#ifdef GL_IMG_multisampled_render_to_texture
	if(strstr((char*)extGL, "GL_IMG_multisampled_render_to_texture"))
	{
		glRenderbufferStorageMultisampleIMG(GL_RENDERBUFFER, SAMPLES, GL_DEPTH_COMPONENT16, w, h);
	}
	else
	{
#endif
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
#ifdef GL_IMG_multisampled_render_to_texture
	}
#endif

	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glGenTextures(1, (GLuint*)&drawTexture);
	glBindTexture(GL_TEXTURE_2D, drawTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, (GLuint*)&frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

#ifdef GL_IMG_multisampled_render_to_texture
	if(strstr((char*)extGL, "GL_IMG_multisampled_render_to_texture"))
	{
		glFramebufferTexture2DMultisampleIMG(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, drawTexture, 0, SAMPLES);
	}
	else
	{
#endif
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, drawTexture, 0);
#ifdef GL_IMG_multisampled_render_to_texture
	}
#endif

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		//What?
		Q_ASSERT(false);
	}

	//Setup VAOs
#ifdef GL_OES_vertex_array_object
	GLint vertexAtt = glGetAttribLocation(circleProgram, "in_vertex");
	if(circleProgram != 0)
	{
		circleVAO = generateCircle(w / 3.0f + 1.0f, vertexAtt, &circleSegCount);
	}
	vertexAtt = glGetAttribLocation(textureProgram, "in_vertex");
	if(textureProgram != 0)
	{
		GLint uvAtt = glGetAttribLocation(textureProgram, "in_texcoord");

		glGenVertexArraysOES(1, (GLuint*)&textureVAO);
		glBindVertexArrayOES(textureVAO);

		GLuint vbos[2];
		glGenBuffers(2, vbos);

		glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
		GLfloat data[4 * 2] = {-w / 2,  h / 2,
								w / 2,  h / 2,
							   -w / 2, -h / 2,
							    w / 2, -h / 2};
		glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
		glVertexAttribPointer(vertexAtt, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(vertexAtt);

		glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
		data[0] = data[4] = data[5] = data[7] = 0.0f;
		data[1] = data[2] = data[3] = data[6] = 1.0f;
		glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
		glVertexAttribPointer(uvAtt, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(uvAtt);

		glBindVertexArrayOES(0);
	}
#endif

	//Clear the screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0F);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	setImage("mona-lisa.png"); //Will clear the screen

	// will initialize as the total of all image data in `source`
	//   because `best` is empty
	totalData = totalDifference(image, frameBuffer, tmpData);

	// the current 'best' starts off as the total
	lowestDifference = totalData;

	initialized = true;
}

#define GENERATE_RANDOM_FLOAT (qrand() / (float)RAND_MAX)

void GeneticCloner::updateImage()
{
	eglMakeCurrent(eglDisp, eglSurf, eglSurf, eglCtx);

	emit mutationsChanged(++totalMutations);

	Color color = Color::fromRGBA(	GENERATE_RANDOM_FLOAT * 255,
									GENERATE_RANDOM_FLOAT * 255,
									GENERATE_RANDOM_FLOAT * 255,
									GENERATE_RANDOM_FLOAT * 0.8 + 0.05);
	int width = glWindowSize.width();
	QPointF loc(GENERATE_RANDOM_FLOAT * width, GENERATE_RANDOM_FLOAT * width);
	float radius = floorf(GENERATE_RANDOM_FLOAT * width / 3.0f + 1.0f);

	float scaleDiv = 1.0f / (width / 3.0f + 1.0f);
	float scale = radius * scaleDiv;

	GLint positionPos = glGetUniformLocation(circleProgram, "un_position");
	GLint scalePos = glGetUniformLocation(circleProgram, "un_scale");
	GLint colorPos = glGetUniformLocation(circleProgram, "un_color");

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(circleProgram);
	glBindVertexArrayOES(circleVAO);
	glUniform2f(positionPos, (GLfloat)loc.x(), (GLfloat)loc.y());
	glUniform1f(scalePos, scale);
	glUniform4f(colorPos, color.red(), color.green(), color.blue(), color.alpha());
	glDrawArrays(GL_LINE_LOOP, 0, circleSegCount);

	long long difference = totalDifference(image, frameBuffer, tmpData);
	if(difference < lowestDifference)
	{
		lowestDifference = difference;
		emit fitnessChanged(fitness());
		emit improvementsChanged(++totalImprovements);

		circles.append(Circle(color, loc, radius));
	}
	else
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		foreach(const Circle& c, circles)
		{
			glUniform2f(positionPos, (GLfloat)c.loc.x(), (GLfloat)c.loc.y());
			glUniform1f(scalePos, c.radius * scaleDiv);
			glUniform4f(colorPos, c.color.red(), c.color.green(), c.color.blue(), c.color.alpha());
			glDrawArrays(GL_LINE_LOOP, 0, circleSegCount);
		}
	}

	glBindVertexArrayOES(0);
	glUseProgram(0);

	drawFramebuffer();

	eglSwapBuffers(eglDisp, eglSurf);
}

/*
gotime = function() {
  var stopped, timer_id;
  var totalMutations, totalImprovements;
  var best_canvas, mutation_canvas;

  var totalData, lowestDifference;

  var circles;

  var WIDTH = 100;

  loadImage('mona-lisa.png');

  function init() {

    stopped = 1;
    timer_id;

    totalMutations = 0;
    totalImprovements = 0;

    // store the canvases
    best_canvas = document.getElementById('best');
    mutation_canvas = document.getElementById('mutation');

    // clear best
    ctx = best_canvas.getContext('2d');
    ctx.clearRect(0, 0, WIDTH, WIDTH);

    // clear mutate
    ctx = mutation_canvas.getContext('2d');
    ctx.clearRect(0, 0, WIDTH, WIDTH);

    // will initialize as the total of all image data in `source`
    //   because `best` is empty
    totalData = totalDifference('source', 'best');

    // the current 'best' starts off as the total
    lowestDifference = totalData;

    // storage of our circles
    // our "DNA"
    circles = [];
  }

  function loadImage(path) {
    // swap the IMG tag
    var img = document.getElementById('source_image');
    img.src = path;
    initialzed = false;

    // fill the source canvas
    var ctx = document.getElementById('source').getContext('2d');
    ctx.drawImage(img, 0, 0);
  }

  document.getElementById('load_image_form').onsubmit = function(e) {
    e.preventDefault();

    var path = document.getElementById('source_image_path').value;
    loadImage(path);
  }

  // computes a total of all the differences in pixels for two canvases
  function totalDifference(canvas1, canvas2) {
    var ctx1 = document.getElementById(canvas1).getContext('2d');
    var data1 = ctx1.getImageData(0, 0, WIDTH, WIDTH).data;

    var ctx2 = document.getElementById(canvas2).getContext('2d');
    var data2 = ctx2.getImageData(0, 0, WIDTH, WIDTH).data;

    var total = 0;
    for (var i = 0; i < WIDTH * WIDTH * 4; i+=4)
      total += Math.abs(data1[i] - data2[i]) + Math.abs(data1[i+1] - data2[i+1]) + Math.abs(data1[i+2] - data2[i+2]);

    return total;
  }

  // draws a circle object using a given canvas
  function drawCircle(circle, canvas) {
    var ctx = canvas.getContext('2d');

    // set the fill style
    ctx.fillStyle = 'rgba(' + [circle.r, circle.g, circle.b, circle.a].join(',') + ')';

    // draw the circle with the given parameters
    ctx.beginPath();
    ctx.arc(circle.x, circle.y, circle.radius, 0, Math.PI*2, true);
    ctx.closePath();
    ctx.fill();
  }

  // strategy
  //   generate a random circle (random R, G, B, A, radius, x, y)
  //   draw it
  //   compute the difference
  //   if it's lower than `lowestDifference`
  //     add the circle to `circles`
  //     set `lowestDifference`
  //     update `best`
  //   else
  //     restore the canvas

  function mutate() {
    totalMutations++;

    var mutation = {
      r : Math.floor(Math.random() * 256),
      g : Math.floor(Math.random() * 256),
      b : Math.floor(Math.random() * 256),

      a : Math.random() * 0.8 + 0.05,

      x : Math.floor(Math.random() * WIDTH),
      y : Math.floor(Math.random() * WIDTH),

      radius : Math.floor(Math.random() * WIDTH / 3 + 1)
    };

    drawCircle(mutation, mutation_canvas);
    var difference = totalDifference('source', 'mutation');
    if (difference < lowestDifference) {
      lowestDifference = difference;
      circles.push(mutation);

      drawCircle(mutation, best_canvas);
      totalImprovements++;
    } else {
      var ctx = mutation_canvas.getContext('2d');
      ctx.clearRect(0, 0, WIDTH, WIDTH);
      for (var i in circles) {
        drawCircle(circles[i], mutation_canvas);
      }
    }

    document.getElementById('improvements').innerHTML = totalImprovements;
    document.getElementById('mutations').innerHTML = totalMutations;

    var fitness = (1 - lowestDifference / totalData) * 100;
    document.getElementById('fitness').innerHTML = fitness.toFixed(2);

  }

  initialzed = false;

  document.getElementById('start_button').onclick = function(e) {
    e.preventDefault();

    if (!initialzed) {
      init();
      initialzed = true;
    }

    if (stopped) {
      timer_id = setInterval(mutate, 0);
      this.innerHTML = 'stop';
      stopped = 0;
    } else {
      clearInterval(timer_id);
      this.innerHTML = 'start';
      stopped = 1;
    }

    mutate();

  }
}
*/
