#include <Eigen/Core>
#include "CinderImGui.h"
#include "CinderLibIgl.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/GeomIo.h"
#include "cinder/ImageIo.h"
#include "cinder/Log.h"
#include "cinder/ObjLoader.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/VboMesh.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"
// #include <igl/opengl/create_mesh_vbo.h>
#include <igl/read_triangle_mesh.h>
#include <igl/viewer/ViewerData.h>
#include "Grpah/ngraph.hpp"
#include "nfd.h"

using namespace ci;
using namespace ci::app;
using namespace std;

void prepareSettings(App::Settings* settings);

class GeometryApp : public App {
 public:
  GeometryApp();

  enum Primitive {
    CAPSULE,
    CONE,
    CUBE,
    CYLINDER,
    HELIX,
    ICOSAHEDRON,
    ICOSPHERE,
    SPHERE,
    TEAPOT,
    TORUS,
    TORUSKNOT,
    PLANE,
    RECT,
    ROUNDEDRECT,
    CIRCLE,
    RING,
    VBOMESH,
    PRIMITIVE_COUNT
  };
  enum Quality { LOW, DEFAULT, HIGH };
  enum ViewMode { PHONG, WIREFRAME, LAMBERT };
  enum TexturingMode { NONE, PROCEDURAL, SAMPLER };

  void setup() override;
  void resize() override;
  void update() override;
  void draw() override;

  void mouseDown(MouseEvent event) override;
  void mouseDrag(MouseEvent event) override;
  void keyDown(KeyEvent event) override;

  void fileDrop(FileDropEvent event) override;

  void initUI();
  void drawUI();

  void testGraph();
  void testNfd();

 private:
  void createGrid();
  void createLambertShader();
  void createPhongShader();
  void createWireShader();
  void createWireframeShader();
  void createGeometry();
  void loadGeomSource(const geom::Source& source,
                      const geom::Source& sourceWire);
  // void createParams();
  // void updateParams();

  Color mBackGroundColor = Color::white();

  Primitive mPrimitiveSelected;
  Primitive mPrimitiveCurrent;
  Quality mQualitySelected;
  Quality mQualityCurrent;
  ViewMode mViewMode;
  int mTexturingMode;
  Color mPrimitiveColor;
  Color mWireColor;

  bool mShowColors;
  bool mShowNormals, mShowTangents;
  bool mShowGrid; /**< Identifier of enabling showing grid*/
  bool mShowSolidPrimitive;
  bool mShowWirePrimitive; /**< Flag m Show Wire Primitive  */
  bool mEnableFaceFulling;

  CameraPersp mCamera;
  CameraUi mCamUi;
  bool mRecenterCamera;
  vec3 mCameraTarget, mCameraLerpTarget, mCameraViewDirection;
  double mLastMouseDownTime;
  vec3 mCamRight, mCamUp;

  gl::VertBatchRef mGrid;

  gl::BatchRef mPrimitive;
  gl::BatchRef mPrimitiveLambert;
  gl::BatchRef mPrimitiveWire;
  gl::BatchRef mPrimitiveWireframe;
  gl::BatchRef mPrimitiveNormalLines, mPrimitiveTangentLines;

  gl::GlslProgRef mPhongShader;
  gl::GlslProgRef mLambertShader;
  gl::GlslProgRef mWireShader;
  gl::GlslProgRef mWireframeShader;

  gl::TextureRef mTexture;

  float mCapsuleRadius;
  float mCapsuleLength;

  float mConeRatio;

  float mHelixRatio;
  unsigned mHelixTwist;
  float mHelixOffset;
  float mHelixCoils;

  float mRingWidth;

  float mRoundedRectRadius;

  unsigned mTorusTwist;
  float mTorusOffset;
  float mTorusRatio;

  int mTorusKnotP;
  int mTorusKnotQ;
  float mTorusKnotRadius;
  vec3 mTorusKnotScale;

  IglMesh triMesh;

  // #if ! defined( CINDER_GL_ES )
  // 	params::InterfaceGlRef	mParams;

  // 	typedef std::vector<params::InterfaceGl::OptionsBase> ParamGroup;
  // 	std::vector<ParamGroup>	mPrimitiveParams;
  // #endif
};

void prepareSettings(App::Settings* settings) {
  settings->setWindowSize(1024, 768);
  settings->setHighDensityDisplayEnabled();
  settings->setMultiTouchEnabled(false);
  settings->disableFrameRate();
}

GeometryApp::GeometryApp()
    : mCapsuleRadius(0.5f),
      mCapsuleLength(1),
      mConeRatio(0.5f),
      mHelixRatio(0.25f),
      mHelixTwist(0),
      mHelixOffset(0),
      mHelixCoils(3),
      mRingWidth(0.25f),
      mRoundedRectRadius(0.2f),
      mTorusTwist(0),
      mTorusOffset(0),
      mTorusRatio(0.25),
      mTorusKnotP(2),
      mTorusKnotQ(5),
      mTorusKnotRadius(0.15f),
      mTorusKnotScale(1, 0.2, 1) {}

void GeometryApp::setup() {
  // gl::enableVerticalSync( true );
  gl::enableVerticalSync(false);
  disableFrameRate();

  // Initialize variables.
  mPrimitiveSelected = mPrimitiveCurrent = VBOMESH;
  mQualitySelected = mQualityCurrent = HIGH;
  mTexturingMode = PROCEDURAL;
  mViewMode = LAMBERT;
  mPrimitiveColor = Color::white();
  mWireColor = Color::black();
  mLastMouseDownTime = 0;
  mShowColors = false;
  mShowNormals = false;
  mShowTangents = false;
  mShowGrid = true;
  mShowSolidPrimitive = true;
  mShowWirePrimitive = false;
  mEnableFaceFulling = false;

  // Load the textures.
  gl::Texture::Format fmt;
  fmt.setAutoInternalFormat();
  fmt.setWrap(GL_REPEAT, GL_REPEAT);
  mTexture = gl::Texture::create(loadImage(loadAsset("stripes.jpg")), fmt);

  // Setup the camera.
  mCamera.lookAt(normalize(vec3(3, 3, 6)) * 5.0f, mCameraTarget);
  mCamUi = CameraUi(&mCamera);

  // Load and compile the shaders.
  createLambertShader();
  createPhongShader();
  createWireShader();
  createWireframeShader();

  // Create the meshes.
  createGrid();
  createGeometry();

  // Create a parameter window, so we can toggle stuff.
  // createParams();
  // triMesh =
  //     IglMesh("/home/origamidance/dependencies/libigl/tutorial/shared/cow.off");

  initUI();
}

void GeometryApp::update() {
  // If another primitive or quality was selected, reset the subdivision and
  // recreate the primitive.
  if (mPrimitiveCurrent != mPrimitiveSelected ||
      mQualitySelected != mQualityCurrent) {
    mPrimitiveCurrent = mPrimitiveSelected;
    mQualityCurrent = mQualitySelected;
    createGeometry();
  }

  // After creating a new primitive, gradually move the camera to get a good
  // view.
  if (mRecenterCamera) {
    float distance = glm::distance(mCamera.getEyePoint(), mCameraLerpTarget);
    vec3 eye =
        mCameraLerpTarget - lerp(distance, 5.0f, 0.25f) * mCameraViewDirection;
    mCameraTarget = lerp(mCameraTarget, mCameraLerpTarget, 0.25f);
    mCamera.lookAt(eye, mCameraTarget);
  }
}

void GeometryApp::draw() {
  // int w = getWindowWidth();
  // int h = getWindowHeight();
  // gl::viewport(0, 0, w / 2, h / 2);
  // Prepare for drawing.
  gl::clear(mBackGroundColor);
  gl::setMatrices(mCamera);
  // cout<<"up direction: "<<mCamera.getWorldUp()<<"\n";
  // Enable the depth buffer.
  gl::enableDepthRead();
  gl::enableDepthWrite();

  if (mPrimitive) {
    gl::ScopedTextureBind scopedTextureBind(mTexture);
    mPhongShader->uniform("uTexturingMode", mTexturingMode);
    mPhongShader->uniform(
        "uFreq", (mPrimitiveCurrent == TORUSKNOT) ? ivec2(100, 10) : ivec2(20));

    // Rotate it slowly around the y-axis.
    gl::ScopedModelMatrix matScope;
    // gl::rotate( float( getElapsedSeconds() / 5 ), 0, 1, 0 );

    // Draw the normals.
    if (mShowNormals && mPrimitiveNormalLines) {
      gl::ScopedColor colorScope(Color(1, 1, 0));
      mPrimitiveNormalLines->draw();
    }

    // Draw the tangents.
    if (mShowTangents && mPrimitiveTangentLines) {
      gl::ScopedColor colorScope(Color(0, 1, 0));
      mPrimitiveTangentLines->draw();
    }

    // Draw the wire primitive.
    // if (mShowWirePrimitive && mPrimitiveWire) {
    if (mShowWirePrimitive) {
      gl::ScopedColor color(mWireColor);
      // gl::ScopedLineWidth linewidth(2.5f);

      gl::enableWireframe();
      glEnable(GL_POLYGON_OFFSET_LINE);
      glPolygonOffset(-1, -1);
      mPrimitiveWire->draw();
      glDisable(GL_POLYGON_OFFSET_LINE);
      gl::disableWireframe();
    }

    // Draw the primitive.
    if (mShowSolidPrimitive) {
      gl::ScopedColor colorScope(Color(1, 1, 1));

      if (mViewMode == WIREFRAME) {
        // We're using alpha blending, so render the back side first.
        gl::ScopedBlendAlpha blendScope;
        gl::ScopedFaceCulling cullScope(true, GL_FRONT);

        mWireframeShader->uniform("uBrightness", 0.5f);
        mPrimitiveWireframe->draw();

        // Now render the front side.
        gl::cullFace(GL_BACK);

        mWireframeShader->uniform("uBrightness", 1.0f);
        mPrimitiveWireframe->draw();
      } else if (mViewMode == PHONG) {
        gl::ScopedFaceCulling cullScope(mEnableFaceFulling, GL_BACK);
        gl::ScopedColor colorScope(mPrimitiveColor);
        mPrimitive->draw();
      } else {
        gl::ScopedFaceCulling cullScope(mEnableFaceFulling, GL_BACK);
        gl::ScopedColor colorScope(mPrimitiveColor);
        mPrimitiveLambert->draw();
      }
    }
  }

  //
  gl::disableDepthWrite();

  // Draw the grid.
  if (mShowGrid && mGrid) {
    gl::ScopedGlslProg scopedGlslProg(
        gl::context()->getStockShader(gl::ShaderDef().color()));

    mGrid->draw();

    // draw the coordinate frame with length 2.
    gl::lineWidth(3);
    gl::drawCoordinateFrame(2);
    gl::lineWidth(1);
  }

  // Disable the depth buffer.
  gl::disableDepthRead();
  // gl::viewport(0, 0, w, h);
  drawUI();
}

void GeometryApp::mouseDown(MouseEvent event) {
  mRecenterCamera = false;

  mCamera.getBillboardVectors(&mCamRight, &mCamUp);
  mCamera.setWorldUp(mCamUp);
  mCamUi.mouseDown(event);

  if (getElapsedSeconds() - mLastMouseDownTime < 0.2f) {
    mPrimitiveSelected =
        static_cast<Primitive>(static_cast<int>(mPrimitiveSelected) + 1);
    createGeometry();
  }

  mLastMouseDownTime = getElapsedSeconds();
}

void GeometryApp::mouseDrag(MouseEvent event) {
  mCamUi.mouseDrag(event);
}

void GeometryApp::resize() {
  mCamera.setAspectRatio(getWindowAspectRatio());

  if (mWireframeShader)
    mWireframeShader->uniform("uViewportSize", vec2(getWindowSize()));
}

void GeometryApp::keyDown(KeyEvent event) {
  switch (event.getCode()) {
    case KeyEvent::KEY_SPACE:
      mPrimitiveSelected = static_cast<Primitive>(
          (static_cast<int>(mPrimitiveSelected) + 1) % PRIMITIVE_COUNT);
      createGeometry();
      // updateParams();
      break;
    case KeyEvent::KEY_c:
      mShowColors = !mShowColors;
      createGeometry();
      break;
    case KeyEvent::KEY_n:
      mShowNormals = !mShowNormals;
      break;
    case KeyEvent::KEY_g:
      mShowGrid = !mShowGrid;
      break;
    case KeyEvent::KEY_q:
      mQualitySelected = Quality((int)(mQualitySelected + 1) % 3);
      break;
#if !defined(CINDER_GL_ES)
    case KeyEvent::KEY_v:
      // if (mViewMode == WIREFRAME)
      //   mViewMode = SHADED;
      // else
      //   mViewMode = WIREFRAME;
      mViewMode = ViewMode((int)(mViewMode + 1) % 3);
      break;
#endif
    case KeyEvent::KEY_w:
      mShowWirePrimitive = !mShowWirePrimitive;
      break;
    case KeyEvent::KEY_RETURN:
      CI_LOG_V("reload");
      createPhongShader();
      createGeometry();
      break;
  }
}

void GeometryApp::fileDrop(FileDropEvent event) {
  try {
    gl::Texture::Format fmt;
    fmt.setAutoInternalFormat();
    fmt.setWrap(GL_REPEAT, GL_REPEAT);

    mTexture = gl::Texture2d::create(loadImage(event.getFile(0)), fmt);
  } catch (const std::exception& exc) {
  }
}

void GeometryApp::createGrid() {
  mGrid = gl::VertBatch::create(GL_LINES);
  mGrid->begin(GL_LINES);
  for (int i = -10; i <= 10; ++i) {
    mGrid->color(Color(0.25f, 0.25f, 0.25f));
    mGrid->color(Color(0.25f, 0.25f, 0.25f));
    mGrid->color(Color(0.25f, 0.25f, 0.25f));
    mGrid->color(Color(0.25f, 0.25f, 0.25f));

    mGrid->vertex(float(i), 0.0f, -10.0f);
    mGrid->vertex(float(i), 0.0f, +10.0f);
    mGrid->vertex(-10.0f, 0.0f, float(i));
    mGrid->vertex(+10.0f, 0.0f, float(i));
  }
  mGrid->end();
}

void GeometryApp::createGeometry() {
  geom::SourceRef primitive;

  switch (mPrimitiveCurrent) {
    default:
      mPrimitiveSelected = CAPSULE;
    case CAPSULE:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(
              geom::Capsule().radius(mCapsuleRadius).length(mCapsuleLength),
              geom::WireCapsule()
                  .radius(mCapsuleRadius)
                  .length(mCapsuleLength));
          break;
        case LOW:
          loadGeomSource(geom::Capsule()
                             .radius(mCapsuleRadius)
                             .length(mCapsuleLength)
                             .subdivisionsAxis(6)
                             .subdivisionsHeight(1),
                         geom::WireCapsule()
                             .radius(mCapsuleRadius)
                             .length(mCapsuleLength));
          break;
        case HIGH:
          loadGeomSource(geom::Capsule()
                             .radius(mCapsuleRadius)
                             .length(mCapsuleLength)
                             .subdivisionsAxis(60)
                             .subdivisionsHeight(20),
                         geom::WireCapsule()
                             .radius(mCapsuleRadius)
                             .length(mCapsuleLength));
          break;
      }
      break;
    case CONE:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Cone().ratio(mConeRatio), geom::WireCone());
          break;
        case LOW:
          loadGeomSource(geom::Cone()
                             .ratio(mConeRatio)
                             .subdivisionsAxis(6)
                             .subdivisionsHeight(1),
                         geom::WireCone());
          break;
        case HIGH:
          loadGeomSource(geom::Cone()
                             .ratio(mConeRatio)
                             .subdivisionsAxis(60)
                             .subdivisionsHeight(60),
                         geom::WireCone());
          break;
      }
      break;
    case CUBE:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Cube(), geom::WireCube());
          break;
        case LOW:
          loadGeomSource(geom::Cube().subdivisions(1), geom::WireCube());
          break;
        case HIGH:
          loadGeomSource(geom::Cube().subdivisions(10), geom::WireCube());
          break;
      }
      break;
    case CYLINDER:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Cylinder(), geom::WireCylinder());
          break;
        case LOW:
          loadGeomSource(
              geom::Cylinder().subdivisionsAxis(6).subdivisionsCap(1),
              geom::WireCylinder());
          break;
        case HIGH:
          loadGeomSource(geom::Cylinder()
                             .subdivisionsAxis(60)
                             .subdivisionsHeight(20)
                             .subdivisionsCap(10),
                         geom::WireCylinder());
          break;
      }
      break;
    case HELIX:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Helix()
                             .ratio(mHelixRatio)
                             .coils(mHelixCoils)
                             .twist(mHelixTwist, mHelixOffset),
                         geom::WireCube());
          break;
        case LOW:
          loadGeomSource(geom::Helix()
                             .ratio(mHelixRatio)
                             .coils(mHelixCoils)
                             .twist(mHelixTwist, mHelixOffset)
                             .subdivisionsAxis(12)
                             .subdivisionsHeight(6),
                         geom::WireCube());
          break;
        case HIGH:
          loadGeomSource(geom::Helix()
                             .ratio(mHelixRatio)
                             .coils(mHelixCoils)
                             .twist(mHelixTwist, mHelixOffset)
                             .subdivisionsAxis(60)
                             .subdivisionsHeight(60),
                         geom::WireCube());
          break;
      }
      break;
    case ICOSAHEDRON:
      loadGeomSource(geom::Icosahedron(), geom::WireIcosahedron());
      break;
    case ICOSPHERE:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Icosphere(), geom::WireSphere());
          break;
        case LOW:
          loadGeomSource(geom::Icosphere().subdivisions(1), geom::WireSphere());
          break;
        case HIGH:
          loadGeomSource(geom::Icosphere().subdivisions(5), geom::WireSphere());
          break;
      }
      break;
    case SPHERE:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Sphere(), geom::WireSphere());
          break;
        case LOW:
          loadGeomSource(geom::Sphere().subdivisions(6), geom::WireSphere());
          break;
        case HIGH:
          loadGeomSource(geom::Sphere().subdivisions(60), geom::WireSphere());
          break;
      }
      break;
    case TEAPOT:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Teapot(), geom::WireCube());
          break;
        case LOW:
          loadGeomSource(geom::Teapot().subdivisions(2), geom::WireCube());
          break;
        case HIGH:
          loadGeomSource(geom::Teapot().subdivisions(12), geom::WireCube());
          break;
      }
      break;
    case TORUS:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(
              geom::Torus().twist(mTorusTwist, mTorusOffset).ratio(mTorusRatio),
              geom::WireTorus().ratio(mTorusRatio));
          break;
        case LOW:
          loadGeomSource(geom::Torus()
                             .twist(mTorusTwist, mTorusOffset)
                             .ratio(mTorusRatio)
                             .subdivisionsAxis(12)
                             .subdivisionsHeight(6),
                         geom::WireTorus().ratio(mTorusRatio));
          break;
        case HIGH:
          loadGeomSource(geom::Torus()
                             .twist(mTorusTwist, mTorusOffset)
                             .ratio(mTorusRatio)
                             .subdivisionsAxis(60)
                             .subdivisionsHeight(60),
                         geom::WireTorus().ratio(mTorusRatio));
          break;
      }
      break;
    case TORUSKNOT:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::TorusKnot()
                             .parameters(mTorusKnotP, mTorusKnotQ)
                             .radius(mTorusKnotRadius)
                             .scale(mTorusKnotScale),
                         geom::WireCube());
          break;
        case LOW:
          loadGeomSource(geom::TorusKnot()
                             .parameters(mTorusKnotP, mTorusKnotQ)
                             .radius(mTorusKnotRadius)
                             .scale(mTorusKnotScale)
                             .subdivisionsAxis(6)
                             .subdivisionsHeight(64),
                         geom::WireCube());
          break;
        case HIGH:
          loadGeomSource(geom::TorusKnot()
                             .parameters(mTorusKnotP, mTorusKnotQ)
                             .radius(mTorusKnotRadius)
                             .scale(mTorusKnotScale)
                             .subdivisionsAxis(32)
                             .subdivisionsHeight(1024),
                         geom::WireCube());
          break;
      }
      break;
    case PLANE:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Plane().subdivisions(ivec2(10, 10)),
                         geom::WirePlane().subdivisions(ivec2(10, 10)));
          break;
        case LOW:
          loadGeomSource(geom::Plane().subdivisions(ivec2(2, 2)),
                         geom::WirePlane().subdivisions(ivec2(2, 2)));
          break;
        case HIGH:
          loadGeomSource(geom::Plane().subdivisions(ivec2(100, 100)),
                         geom::WirePlane().subdivisions(ivec2(100, 100)));
          break;
      }
      break;

    case RECT:
      loadGeomSource(geom::Rect(), geom::WirePlane());
      break;
      break;
    case ROUNDEDRECT:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::RoundedRect()
                             .cornerRadius(mRoundedRectRadius)
                             .cornerSubdivisions(3),
                         geom::WireRoundedRect()
                             .cornerRadius(mRoundedRectRadius)
                             .cornerSubdivisions(3));
          break;
        case LOW:
          loadGeomSource(geom::RoundedRect()
                             .cornerRadius(mRoundedRectRadius)
                             .cornerSubdivisions(1),
                         geom::WireRoundedRect()
                             .cornerRadius(mRoundedRectRadius)
                             .cornerSubdivisions(1));
          break;
        case HIGH:
          loadGeomSource(geom::RoundedRect()
                             .cornerRadius(mRoundedRectRadius)
                             .cornerSubdivisions(9),
                         geom::WireRoundedRect()
                             .cornerRadius(mRoundedRectRadius)
                             .cornerSubdivisions(9));
          break;
      }
      break;
    case CIRCLE:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Circle().subdivisions(24),
                         geom::WireCircle().subdivisions(24));
          break;
        case LOW:
          loadGeomSource(geom::Circle().subdivisions(8),
                         geom::WireCircle().subdivisions(8));
          break;
        case HIGH:
          loadGeomSource(geom::Circle().subdivisions(120),
                         geom::WireCircle().subdivisions(120));
          break;
      }
      break;
    case RING:
      switch (mQualityCurrent) {
        case DEFAULT:
          loadGeomSource(geom::Ring().width(mRingWidth).subdivisions(24),
                         geom::WireCircle().subdivisions(24).radius(
                             1 + 0.5f * mRingWidth));
          break;
        case LOW:
          loadGeomSource(
              geom::Ring().width(mRingWidth).subdivisions(8),
              geom::WireCircle().subdivisions(8).radius(1 + 0.5f * mRingWidth));
          break;
        case HIGH:
          loadGeomSource(geom::Ring().width(mRingWidth).subdivisions(120),
                         geom::WireCircle().subdivisions(120).radius(
                             1 + 0.5f * mRingWidth));
          break;
      }
      break;
    case VBOMESH:
      // IglMesh triMesh = IglMesh(getAssetPath("Monster_remesh.stl").string());
      mPrimitiveWire = gl::Batch::create(
          triMesh, gl::getStockShader(gl::ShaderDef().color()));
      if (mPhongShader) {
        mPrimitive = gl::Batch::create(triMesh, mPhongShader);
      }
      if (mLambertShader) {
        mPrimitiveLambert = gl::Batch::create(triMesh, mLambertShader);
      }
      if (mWireframeShader) {
        mPrimitiveWireframe = gl::Batch::create(triMesh, mWireframeShader);
      }
      break;
  }
}

void GeometryApp::loadGeomSource(const geom::Source& source,
                                 const geom::Source& sourceWire) {
  // The purpose of the TriMesh is to capture a bounding box; without that need
  // we could just instantiate the Batch directly using primitive
  TriMesh::Format fmt =
      TriMesh::Format().positions().normals().texCoords().tangents();
  if (mShowColors && source.getAvailableAttribs().count(geom::COLOR) > 0)
    fmt.colors();

  TriMesh mesh(source, fmt);
  AxisAlignedBox bbox = mesh.calcBoundingBox();
  mCameraLerpTarget = mesh.calcBoundingBox().getCenter();
  mCameraViewDirection = mCamera.getViewDirection();
  mRecenterCamera = true;

  if (mPhongShader)
    mPrimitive = gl::Batch::create(mesh, mPhongShader);

  if (mLambertShader)
    mPrimitiveLambert = gl::Batch::create(mesh, mLambertShader);

  if (mWireShader)
    // mPrimitiveWire = gl::Batch::create(sourceWire, mWireShader);
    mPrimitiveWire =
        gl::Batch::create(mesh, gl::getStockShader(gl::ShaderDef().color()));

  if (mWireframeShader)
    mPrimitiveWireframe = gl::Batch::create(mesh, mWireframeShader);

  vec3 size = bbox.getMax() - bbox.getMin();
  float scale = std::max(std::max(size.x, size.y), size.z) / 25.0f;
  mPrimitiveNormalLines =
      gl::Batch::create(mesh >> geom::VertexNormalLines(scale),
                        gl::getStockShader(gl::ShaderDef().color()));
  if (mesh.hasTangents())
    mPrimitiveTangentLines =
        gl::Batch::create(mesh >> geom::VertexNormalLines(scale, geom::TANGENT),
                          gl::getStockShader(gl::ShaderDef().color()));
  else
    mPrimitiveTangentLines.reset();

  getWindow()->setTitle("Geometry - " + to_string(mesh.getNumVertices()) +
                        " vertices - " +
                        to_string(sourceWire.getNumVertices() / 2) + " lines ");
}

void GeometryApp::createLambertShader() {
  try {
    mLambertShader = gl::getStockShader(gl::ShaderDef().lambert().color());
  } catch (Exception& exc) {
    CI_LOG_E("error loading lambert sader:" << exc.what());
  }
}

void GeometryApp::createPhongShader() {
  try {
#if defined(CINDER_GL_ES)
    mPhongShader = gl::GlslProg::create(loadAsset("phong_es2.vert"),
                                        loadAsset("phong_es2.frag"));
#else
    mPhongShader =
        gl::GlslProg::create(loadAsset("phong.vert"), loadAsset("phong.frag"));
#endif
  } catch (Exception& exc) {
    CI_LOG_E("error loading phong shader: " << exc.what());
  }
}

void GeometryApp::createWireShader() {
  try {
    mWireShader = gl::context()->getStockShader(gl::ShaderDef().color());
  } catch (Exception& exc) {
    CI_LOG_E("error loading wire shader: " << exc.what());
  }
}

void GeometryApp::createWireframeShader() {
#if !defined(CINDER_GL_ES)
  try {
    auto format = gl::GlslProg::Format()
                      .vertex(loadAsset("wireframe.vert"))
                      .geometry(loadAsset("wireframe.geom"))
                      .fragment(loadAsset("wireframe.frag"));

    mWireframeShader = gl::GlslProg::create(format);
  } catch (Exception& exc) {
    CI_LOG_E("error loading wireframe shader: " << exc.what());
  }
#endif  // ! defined( CINDER_GL_ES )
}

/**
 * @name initUI - Initializes the Imgui
 * @return void
 */
void GeometryApp::initUI() {
  ui::initialize();
}
/**
 * @name drawUI - draws the Imgui
 * @return void
 */
void GeometryApp::drawUI() {
  // The view options
  ui::ShowTestWindow();
  {
    vector<string> primitives = {"Capsule",    "Cone",
                                 "Cube",       "Cylinder",
                                 "Helix",      "Icosahedron",
                                 "Icosphere",  "Sphere",
                                 "Teapot",     "Torus",
                                 "Torus Knot", "Plane",
                                 "Rectangle",  "Rounded Rectangle",
                                 "Circle",     "Ring",
                                 "VBOMesh"};
    vector<string> qualities = {"Low", "Default", "High"};
    // vector<string> viewModes = {"Phong", "Wireframe","Lambert"};
    vector<string> viewModes = {"Phong", "Wireframe", "Lambert"};
    vector<string> texturingModes = {"None", "Procedural", "Sampler"};

    ui::ScopedWindow viewOptions("View options",
                                 ImGuiWindowFlags_AlwaysAutoResize);
    static int nPrimitiveSelected = mPrimitiveSelected;
    if (ui::Combo("Primitive", (int*)&mPrimitiveSelected, primitives)) {
      switch ((int)mPrimitiveSelected) {
        // Capsule
        case CAPSULE:
          if (ui::DragFloat("Capsule: Radius", &mCapsuleRadius, 0.01f)) {
            createGeometry();
          };
          if (ui::DragFloat("Capsule: Length", &mCapsuleLength, 0.01f)) {
            createGeometry();
          };
          break;
        // Cone
        case CONE:
          if (ui::DragFloat("CONE: Ratio", &mConeRatio, 0.01f)) {
            createGeometry();
          };
          break;
        // Helix
        case HELIX:
          if (ui::DragFloat("Helix: Ratio", &mHelixRatio, 0.01f)) {
            createGeometry();
          };
          if (ui::DragFloat("Helix: Coils", &mHelixCoils, 0.01f)) {
            createGeometry();
          };
          if (ui::DragInt("Helix: Twist", (int*)&mHelixTwist, 0.01f)) {
            createGeometry();
          };
          if (ui::DragFloat("Helix: Twist Offset", &mHelixOffset, 0.01f)) {
            createGeometry();
          };
          break;
        // Ring
        case RING:
          if (ui::DragFloat("Ring: Width", &mRingWidth, 0.01f)) {
            createGeometry();
          };
          break;
        // Rounded Rect
        case ROUNDEDRECT:
          if (ui::DragFloat("Corner Radius", &mRoundedRectRadius, 0.01f)) {
            createGeometry();
          };
          break;
        // Torus
        case TORUS:
          if (ui::DragFloat("Torus: Ratio", &mTorusRatio, 0.01f)) {
            createGeometry();
          };
          if (ui::DragInt("Torus: Twist", (int*)&mTorusTwist, 0.01f)) {
            createGeometry();
          };
          if (ui::DragFloat("Torus: Twist Offset", &mTorusOffset, 0.01f)) {
            createGeometry();
          };
          break;
        // Torus Knot
        case TORUSKNOT:
          if (ui::DragInt("Torus Knot: Parameter P", &mTorusKnotP, 0.01f)) {
            createGeometry();
          };
          if (ui::DragInt("Torus Knot: Parameter Q", &mTorusKnotQ, 0.01f)) {
            createGeometry();
          };
          if (ui::DragFloat("Torus Knot: Scale X", &mTorusKnotScale.x, 0.1f)) {
            createGeometry();
          };
          if (ui::DragFloat("Torus Knot: Scale Y", &mTorusKnotScale.y, 0.1f)) {
            createGeometry();
          };
          if (ui::DragFloat("Torus Knot: Scale Z", &mTorusKnotScale.z, 0.1f)) {
            createGeometry();
          };
          if (ui::DragFloat("Torus Knot: Radius", &mTorusKnotRadius, 0.1f)) {
            createGeometry();
          };
          break;
      }
    }
    ui::Combo("Quality", (int*)&mQualitySelected, qualities);
    ui::Combo("Viewing Mode", (int*)&mViewMode, viewModes);
    ui::Combo("Texturing Mode", (int*)&mTexturingMode, texturingModes);
    ui::Checkbox("Show Grid", &mShowGrid);
    ui::Checkbox("Show Normals", &mShowNormals);
    ui::Checkbox("Show Tangents", &mShowTangents);
    if (ui::Checkbox("Show Colors", &mShowColors)) {
      createGeometry();
    };
    if (ui::Checkbox("Face Culling", &mEnableFaceFulling)) {
      gl::enableFaceCulling(mEnableFaceFulling);
    };
    ui::Checkbox("Show Wire Primitive", &mShowWirePrimitive);
    ui::Checkbox("Show Solid Primitive", &mShowSolidPrimitive);
    static ImVec4 bColor = ImColor(mBackGroundColor.r, mBackGroundColor.g,
                                   mBackGroundColor.b, 1.0);
    ui::ColorButton(bColor);
    if (ui::BeginPopupContextItem("background color", 0)) {
      ui::Text("Edit color");
      if (ui::ColorPicker3("", (float*)&mBackGroundColor)) {
        bColor = ImColor(mBackGroundColor.r, mBackGroundColor.g,
                         mBackGroundColor.b, 1.0);
      }
      ImGui::EndPopup();
    }
    ui::SameLine();
    ui::Text("Background Color(click to edit)");

    static ImVec4 pColor =
        ImColor(mPrimitiveColor.r, mPrimitiveColor.g, mPrimitiveColor.b, 1.0);
    ui::ColorButton(pColor);
    if (ui::BeginPopupContextItem("primitive color", 0)) {
      ui::Text("Edit color");
      if (ui::ColorPicker3("", (float*)&mPrimitiveColor)) {
        pColor = ImColor(mPrimitiveColor.r, mPrimitiveColor.g,
                         mPrimitiveColor.b, 1.0);
      }
      ImGui::EndPopup();
    }
    ui::SameLine();
    ui::Text("Primitive Color(click to edit)");
    static ImVec4 wColor =
        ImColor(mWireColor.r, mWireColor.g, mWireColor.b, 1.0);
    ui::ColorButton(wColor);
    if (ui::BeginPopupContextItem("wire color", 0)) {
      ui::Text("Edit color");
      if (ui::ColorPicker3("", (float*)&mWireColor)) {
        wColor = ImColor(mWireColor.r, mWireColor.g, mWireColor.b, 1.0);
      }
      ImGui::EndPopup();
    }
    ui::SameLine();
    ui::Text("Wire Color(click to edit)");
    if (ui::Button("testGraph")) {
      testGraph();
    }
    if (ui::Button("testNfd")) {
      testNfd();
    }
  }
}

void GeometryApp::testGraph() {
  cout << "gotcha!"
       << "\n";
  using namespace NGraph;
  Graph A;
  A.insert_edge(3, 4);
  A.insert_edge(1, 2);
  cout << "graph has:" << A.num_vertices() << "\n";
}

void GeometryApp::testNfd() {
  nfdchar_t* outPath = NULL;
  nfdresult_t result = NFD_OpenDialog("stl,obj,off", NULL, &outPath);
  if (result == NFD_OKAY) {
    puts("Success!");
    puts(outPath);
    // triMesh = IglMesh(string(outPath));
    triMesh.loadMesh(string(outPath));
    cout << "v size=" << triMesh.getV()->size() << "\n";
    // mPrimitiveWire =
    //     gl::Batch::create(triMesh,
    //     gl::getStockShader(gl::ShaderDef().color()));
    // if (mPhongShader) {
    //   mPrimitive = gl::Batch::create(triMesh, mPhongShader);
    // }
    // if (mLambertShader) {
    //   mPrimitiveLambert = gl::Batch::create(triMesh, mLambertShader);
    // }
    // if (mWireframeShader) {
    //   mPrimitiveWireframe = gl::Batch::create(triMesh, mWireframeShader);
    // }
    mPrimitiveSelected = VBOMESH;
    createGeometry();
    free(outPath);
  } else if (result == NFD_CANCEL) {
    puts("User pressed cancel.");
  } else {
    printf("Error: %s\n", NFD_GetError());
  }
}

CINDER_APP(GeometryApp,
           RendererGl(RendererGl::Options().msaa(16)),
           prepareSettings)
