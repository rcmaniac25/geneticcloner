// Default empty project template
#ifndef GeneticCloner_HPP_
#define GeneticCloner_HPP_

#include <bb/cascades/Color>

#include <QObject>
#include <QTimer>
#include <QPointF>
#include <QSize>
#include <QList>

#include <QtGui/QImage>

#include <EGL/egl.h>
#include <screen/screen.h>

namespace bb { namespace cascades { class Application; class ImageView; }}

struct Circle
{
	bb::cascades::Color color;
	QPointF loc;
	float radius;

	Circle(const bb::cascades::Color& color, const QPointF& loc, float radius)
	{
		this->color = color;
		this->loc = loc;
		this->radius = radius;
	}
};

class GeneticCloner : public QObject
{
    Q_OBJECT

    Q_PROPERTY(float fitness READ fitness NOTIFY fitnessChanged FINAL)
    Q_PROPERTY(long long improvements READ improvements NOTIFY improvementsChanged FINAL)
    Q_PROPERTY(long long mutations READ mutations NOTIFY mutationsChanged FINAL)

public:
    GeneticCloner(bb::cascades::Application* app);
    ~GeneticCloner();

    float fitness() const;
    long long improvements() const;
    long long mutations() const;

    Q_INVOKABLE void toggleRunning();

    void init(bb::cascades::Application* app);

Q_SIGNALS:
	void fitnessChanged(float value);
	void improvementsChanged(long long value);
	void mutationsChanged(long long value);

private Q_SLOTS:
	void updateImage();
	void setImage(const QString& path);

private:
	bool initialized;
	long long totalMutations;
	long long totalImprovements;
	long long totalData;
	long long lowestDifference;
	QList<Circle> circles;
	QSize glWindowSize;
	uchar* tmpData;
	QString imagePath;

	bb::cascades::ImageView* originalImage;
	QTimer* timer;
	QImage* image;

	screen_context_t context;
	screen_window_t window;

	EGLDisplay eglDisp;
	EGLSurface eglSurf;

	EGLContext eglCtx;
	EGLConfig eglConf;

	int frameBuffer;
	int renderbuffer;
	int drawTexture;
	int circleProgram;
	int textureProgram;
	int circleSegCount;
	int circleVAO;
	int textureVAO;
	int tempTexture; //XXX

	void drawFramebuffer();
	void clearMutationWindow();
	void createForeignWindow(const QString& group, const QString& id, int width, int height, int usage, int format);

	Q_DISABLE_COPY(GeneticCloner)
};


#endif
