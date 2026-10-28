#pragma once
#include "Spectrum.h"
#include "Appearance.h"
#include "Trajectory.h"
#include "BandSelection.h"
namespace fluid {
class SpectrumPanel : public juce::Component, private juce::OpenGLRenderer {
    SpectrumAnalyser& analyser;
    Capture* xyCapture=nullptr;
    std::shared_ptr<const TrajectoryFrame> trajectoryFrame,uploadedTrajectory;
    unsigned waveTexture=0;int waveRows=0;
    juce::AudioProcessorValueTreeState& state;
    std::atomic<const void*>* previewOwner=nullptr;
    juce::OpenGLContext context;
    bool threeD=false,selecting=false,rotating=false;
    float anchor=20,hover=-1,startYaw=0,startPitch=35;
    juce::Point<float> dragStart;
    enum class BandDrag {create,move,low,high};
    BandDrag bandDrag=BandDrag::create;
    float startBandLow=20,startBandHigh=20000;
    bool dragLogarithmic=true;
    std::atomic<int> canvasWidth{1},canvasHeight{1};
    std::atomic<bool> ready{false},failed{false};
    std::atomic<unsigned> renderCount{0},updatedCount{0};
    unsigned renderedSequence=0;
    std::atomic<bool> captureRequested{false};
    std::shared_ptr<const juce::Image> captured,overlayImage,uploadedOverlay;
    juce::OpenGLTexture overlayTexture;
    std::array<float,19> overlayKey{};
    unsigned lastSequence=0;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> position,colour;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> camera,surface,fade,xyView;
    std::unique_ptr<juce::OpenGLShaderProgram> lineShader;
    using Uniform=juce::OpenGLShaderProgram::Uniform;
    std::unique_ptr<Uniform> lineCamera,lineInfo,lineY,lineOsc,lineStroke,lineColour,lineFade,lineWave;
    unsigned vao=0,vbo=0,ibo=0;
    struct Vertex{float x,y,z,r,g,b;};
    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    float value(const char* id) const {return state.getRawParameterValue(id)->load();}
    bool listening() const {return value("listen")>.5f||(previewOwner&&previewOwner->load()!=nullptr);}
    void temporaryPreview(bool enabled) {
        if(!previewOwner)return;
        if(enabled)previewOwner->store(this);
        else {const void* expected=this;previewOwner->compare_exchange_strong(expected,nullptr);}
    }
    void endBandGesture() {
        temporaryPreview(false);
        if(selecting||rotating)for(auto* id:(rotating?std::initializer_list<const char*>{yawID(),pitchID()}:std::initializer_list<const char*>{"bandLow","bandHigh"}))state.getParameter(id)->endChangeGesture();
        selecting=rotating=false;
    }
    void set(const char* id,float v) {auto* p=state.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(v));}
    const char* yawID()const{return xyCapture?"xyOrbitYaw":"historyOrbitYaw";}
    const char* pitchID()const{return xyCapture?"xyOrbitPitch":"historyOrbitPitch";}
    const char* baseYawID()const{return xyCapture?"xyYaw":"cameraYaw";}
    const char* basePitchID()const{return xyCapture?"xyPitch":"cameraPitch";}
    float angleYaw()const{return value(baseYawID())+value(yawID());}
    float anglePitch()const{return value(basePitchID())+value(pitchID());}
    const char* zoomID()const{return xyCapture?"xyZoom":"historyZoom";}
    Appearance theme() const {return appearance(int(value("style")),int(value("colourL")),int(value("colourR")));}
    float tilt() const {const int w=int(value("fftWeight"));return w==0?0.f:w==1?3.f:w==2?4.5f:value("fftTilt");}
    float ceiling() const {return std::ceil(std::max(6.f,tilt()*5)/6)*6;}
    float normalized(float db,float frequency) const {return std::clamp((db+value("fftGain")+tilt()*std::log2(std::max(20.f,frequency)/1000.f)-value("fftFloor"))/(ceiling()-value("fftFloor")),0.f,1.f);}
    float maxFrequency() const {auto p=analyser.snapshot();return p?p->maxFrequency:20000.f;}
    juce::Rectangle<float> spectrumBox() const {return {48,25,float(getWidth()-68),float(getHeight()-66)};}
    bool bandVisible() const {return value("clean")<.5f&&(value("bandLow")>20.1f||value("bandHigh")<maxFrequency()-.1f||listening());}
    std::array<float,2> bandPixels() const {
        auto box=spectrumBox();float maximum=maxFrequency();bool logarithmic=value("fftScale")<.5f;
        float l=std::clamp(box.getX()+box.getWidth()*spectrumX(std::min(value("bandLow"),value("bandHigh")),logarithmic,maximum),box.getX(),box.getRight());
        float r=std::clamp(box.getX()+box.getWidth()*spectrumX(std::max(value("bandLow"),value("bandHigh")),logarithmic,maximum),l,box.getRight());
        return {l,r};
    }
    juce::Rectangle<float> bandGrip() const {
        auto box=spectrumBox();auto edges=bandPixels();float width=std::clamp(edges[1]-edges[0]-12.f,26.f,42.f);
        return juce::Rectangle<float>(width,24.f).withCentre({(edges[0]+edges[1])*.5f,box.getBottom()-std::min(46.f,box.getHeight()*.22f)});
    }
    BandDrag bandHit(juce::Point<float> p) const {
        if(threeD||!bandVisible()||!spectrumBox().expanded(8,0).contains(p))return BandDrag::create;
        if(bandGrip().expanded(3).contains(p))return BandDrag::move;
        auto edges=bandPixels();float left=std::abs(p.x-edges[0]),right=std::abs(p.x-edges[1]);
        if(std::min(left,right)<=8)return left<=right?BandDrag::low:BandDrag::high;
        return BandDrag::create;
    }
    void bandCursor(juce::Point<float> p) {
        auto hit=bandHit(p);
        setMouseCursor(hit==BandDrag::move?juce::MouseCursor::DraggingHandCursor:
                       hit==BandDrag::low||hit==BandDrag::high?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::NormalCursor);
    }
    static juce::String hz(float f) {return f>=1000?juce::String(f/1000.f, f>=10000?0:1)+"k":juce::String(juce::roundToInt(f));}
    juce::Point<float> project(float x,float y,float age) const {
        const float yaw=angleYaw()*juce::MathConstants<float>::pi/180,pitch=anglePitch()*juce::MathConstants<float>::pi/180;
        const float xx=x*2.1f,yy=y*1.5f-.4f,zz=1.4f*(1-2*age);
        const float rx=xx*std::cos(yaw)+zz*std::sin(yaw),rz=-xx*std::sin(yaw)+zz*std::cos(yaw);
        const float ry=yy*std::cos(pitch)-rz*std::sin(pitch),depth=4.5f-(yy*std::sin(pitch)+rz*std::cos(pitch));
        const float aspect=float(getWidth())/std::max(1,getHeight());
        return {getWidth()*.5f*(1+value(zoomID())*rx*(xyCapture?2.1f/aspect:std::min(2.5f/aspect,1.1f))/depth+.04f),getHeight()*.5f*(1-(value(zoomID())*ry*2.1f/depth+.12f))};
    }
    float mouseFrequency(juce::Point<float> p) const {
        float x=0;
        if(threeD) {auto a=project(-1,0,0),b=project(1,0,0);auto d=b-a;float screen=std::clamp(((p-a).x*d.x+(p-a).y*d.y)/std::max(1.f,d.x*d.x+d.y*d.y),0.f,1.f);
            const float yaw=angleYaw()*juce::MathConstants<float>::pi/180,pitch=anglePitch()*juce::MathConstants<float>::pi/180;
            auto depth=[&](float xx){return 4.5f-(-.4f*std::sin(pitch)+(-xx*std::sin(yaw)+1.4f*std::cos(yaw))*std::cos(pitch));};
            float d0=depth(-2.1f),d1=depth(2.1f);x=screen*d0/(d1-screen*(d1-d0));}
        else x=(p.x-48.f)/std::max(1.f,float(getWidth()-68));
        return spectrumFrequency(x,value("fftScale")<.5f,maxFrequency());
    }
    void select(juce::Point<float> p) {
        const float maximum=maxFrequency(),end=mouseFrequency(p);hover=end;
        const float minimumWidth=std::min(1.f,startBandHigh-startBandLow);
        if(bandDrag==BandDrag::move) {
            auto moved=shiftBand(startBandLow,startBandHigh,(p.x-dragStart.x)/std::max(1.f,spectrumBox().getWidth()),dragLogarithmic,maximum);
            set("bandLow",moved[0]);set("bandHigh",moved[1]);
        } else if(bandDrag==BandDrag::low)set("bandLow",std::clamp(end,20.f,std::max(20.f,startBandHigh-minimumWidth)));
        else if(bandDrag==BandDrag::high)set("bandHigh",std::clamp(end,std::min(maximum,startBandLow+minimumWidth),maximum));
        else {
            const float lo=std::clamp(std::min(anchor,end),20.f,maximum-1);
            set("bandLow",lo);set("bandHigh",std::clamp(std::max(anchor,end),lo+1,maximum));
        }
        repaint();context.triggerRepaint();
    }
    void newOpenGLContextCreated() override {
        using namespace juce::gl;
        context.setSwapInterval(1);
        shader=std::make_unique<juce::OpenGLShaderProgram>(context);
        const char* vs=R"(#version 150
in vec3 position;in vec3 sourceColour;uniform vec4 camera;uniform float xyView;
out vec3 tint;out vec3 viewPosition;out float heat;out float age;noperspective out float edge;
void main(){vec3 p=vec3(position.x*2.1,position.y*1.5-.4,1.4*(1.-2.*position.z));
float cy=cos(camera.x),sy=sin(camera.x),cp=cos(camera.y),sp=sin(camera.y);
float x=p.x*cy+p.z*sy,z=-p.x*sy+p.z*cy,y=p.y*cp-z*sp;z=p.y*sp+z*cp;float d=4.5-z;
gl_Position=vec4(camera.w*x*(xyView>.5?2.1/camera.z:min(2.5/camera.z,1.1))+.04*d,camera.w*y*2.1+.12*d,1.01005*d-.201005,d);
tint=sourceColour;heat=position.y;age=position.z;viewPosition=p;edge=0.;})";
        const char* fs=R"(#version 150
in vec3 tint;in vec3 viewPosition;in float heat;in float age;noperspective in float edge;uniform float surface;uniform float fade;out vec4 frag;
void main(){if(surface>.5 && (surface<1.5||surface>2.5) && heat<.008)discard;float light=1.,alpha=1.;
if(surface>.5){if(fade>.0001)alpha=1.-smoothstep(1.-fade,1.,age);if(surface>1.5)alpha*=1.-smoothstep(.55,1.,abs(edge));if(alpha<.001)discard;}
if(surface>.5 && surface<1.5){vec3 n=normalize(cross(dFdx(viewPosition),dFdy(viewPosition)));light=.62+.38*abs(dot(n,normalize(vec3(.3,.8,.5))));}
frag=vec4(tint*light,alpha);})";
        if(!shader->addVertexShader(vs)||!shader->addFragmentShader(fs)||!shader->link()) {failed.store(true);juce::Logger::writeToLog(shader->getLastError());return;}
        // Project each recorded point once; expand each shared line segment
        // in the geometry stage instead of repeating both endpoints six times.
        if(xyCapture) {
            lineShader=std::make_unique<juce::OpenGLShaderProgram>(context);
            const char* lineVS=R"(#version 150
uniform vec4 camera;uniform sampler2D wave;uniform vec4 xyInfo;uniform vec4 xyY;uniform vec3 xyOsc;
out float sampleAge;
vec3 sampleAt(int i){if(i<0||i>=int(xyOsc.y))return vec3(0.);return texelFetch(wave,ivec2(i%2048,i/2048),0).rgb;}
void main(){int i=int(xyInfo.x)+gl_VertexID;float delayed=float(i)-xyY.y;int k=int(floor(delayed));vec3 v=mix(sampleAt(k),sampleAt(k+1),fract(delayed));
float y=xyY.x>1.5?xyOsc.x*(xyOsc.z>.5?v.z:1.)*sin(6.28318530718*(xyY.w+float(i)*xyY.z)):(xyY.x>.5?v.x:v.y);
sampleAge=(xyInfo.x+xyInfo.y-1.-float(i))/xyInfo.z;
vec3 p=vec3(sampleAt(i).x*xyInfo.w,.2+y*xyInfo.w,1.4*(1.-2.*sampleAge));
float cy=cos(camera.x),sy=sin(camera.x),cp=cos(camera.y),sp=sin(camera.y);
float x=p.x*cy+p.z*sy,z=-p.x*sy+p.z*cy;float yy=p.y*cp-z*sp;z=p.y*sp+z*cp;float d=4.5-z;
gl_Position=vec4(camera.w*x*2.1/camera.z+.04*d,camera.w*yy*2.1+.12*d,1.01005*d-.201005,d);})";
            const char* lineGS=R"(#version 150
layout(lines) in;layout(triangle_strip,max_vertices=4) out;
in float sampleAge[];out float age;noperspective out float edge;
uniform vec3 stroke;
void main(){vec4 a=gl_in[0].gl_Position,b=gl_in[1].gl_Position;
vec2 dir=(b.xy/b.w-a.xy/a.w)*stroke.xy;float length2=dot(dir,dir);if(length2<1.e-12)return;
vec2 n=vec2(-dir.y,dir.x)*inversesqrt(length2)*(stroke.z/stroke.xy);
for(int i=0;i<2;++i){vec4 p=gl_in[i].gl_Position;age=sampleAge[i];edge=1.;gl_Position=p;gl_Position.xy+=n*p.w;EmitVertex();
age=sampleAge[i];edge=-1.;gl_Position=p;gl_Position.xy-=n*p.w;EmitVertex();}EndPrimitive();})";
            const char* lineFS=R"(#version 150
in float age;noperspective in float edge;uniform vec3 lineColour;uniform float fade;out vec4 frag;
void main(){float alpha=(1.-smoothstep(.55,1.,abs(edge)))*(fade>.0001?1.-smoothstep(1.-fade,1.,age):1.);if(alpha<.001)discard;frag=vec4(lineColour,alpha);})";
            if(!lineShader->addVertexShader(lineVS)||!lineShader->addShader(lineGS,GL_GEOMETRY_SHADER)||!lineShader->addFragmentShader(lineFS)||!lineShader->link()){failed.store(true);juce::Logger::writeToLog(lineShader->getLastError());return;}
            auto uniform=[&](const char* name){return std::make_unique<Uniform>(*lineShader,name);};
            lineCamera=uniform("camera");lineInfo=uniform("xyInfo");lineY=uniform("xyY");lineOsc=uniform("xyOsc");lineStroke=uniform("stroke");lineColour=uniform("lineColour");lineFade=uniform("fade");lineWave=uniform("wave");
        }
        position=std::make_unique<juce::OpenGLShaderProgram::Attribute>(*shader,"position");colour=std::make_unique<juce::OpenGLShaderProgram::Attribute>(*shader,"sourceColour");
        camera=std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shader,"camera");surface=std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shader,"surface");
        xyView=std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shader,"xyView");fade=std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shader,"fade");
        glGenTextures(1,&waveTexture);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,waveTexture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);std::array<TrajectoryFrame::Sample,TrajectoryFrame::textureWidth> silence{};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,TrajectoryFrame::textureWidth,1,0,GL_RGBA,GL_FLOAT,silence.data());waveRows=1;glActiveTexture(GL_TEXTURE0);
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glGenBuffers(1,&ibo);
        indices.clear();
        if(!xyCapture)for(int order=0;order<4;++order)for(int row=0;row<SpectrumFrame::rows-1;++row)for(int col=0;col<SpectrumFrame::columns-1;++col) {
            const int r=(order&1)?row:SpectrumFrame::rows-2-row,c=(order&2)?SpectrumFrame::columns-2-col:col;
            unsigned a=unsigned(r*SpectrumFrame::columns+c),b=a+SpectrumFrame::columns;
            for(unsigned i:{a,b,a+1,a+1,b,b+1})indices.push_back(i);
        }
        glBindVertexArray(vao);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(indices.size()*sizeof(unsigned)),indices.data(),GL_STATIC_DRAW);
        glBindVertexArray(0);ready.store(true);
    }
    void openGLContextClosing() override {
        using namespace juce::gl;
        ready.store(false);lineCamera.reset();lineInfo.reset();lineY.reset();lineOsc.reset();lineStroke.reset();lineColour.reset();lineFade.reset();lineWave.reset();lineShader.reset();overlayTexture.release();uploadedOverlay.reset();uploadedTrajectory.reset();waveRows=0;fade.reset();xyView.reset();if(waveTexture)glDeleteTextures(1,&waveTexture);waveTexture=0;position.reset();colour.reset();camera.reset();surface.reset();shader.reset();
        if(ibo)glDeleteBuffers(1,&ibo);if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);ibo=vbo=vao=0;
    }
    void renderOpenGL() override {
        using namespace juce::gl;
        auto a=theme();juce::OpenGLHelpers::clear(a.panel);if(!ready.load())return;
        auto frame=analyser.snapshot();auto trajectory=std::atomic_load(&trajectoryFrame);
        if(xyCapture?!trajectory:!frame)return;
        const int width=canvasWidth.load(),height=canvasHeight.load();
        glViewport(0,0,juce::roundToInt(width*context.getRenderingScale()),juce::roundToInt(height*context.getRenderingScale()));
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
        shader->use();camera->set(angleYaw()*juce::MathConstants<float>::pi/180,anglePitch()*juce::MathConstants<float>::pi/180,float(width)/std::max(1,height),value(zoomID()));
        fade->set(value("rearFade")*.01f);xyView->set(xyCapture?1.f:0.f);
        const bool logarithmic=value("fftScale")<.5f;const float low=std::min(value("bandLow"),value("bandHigh")),high=std::max(value("bandLow"),value("bandHigh"));
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);
        glVertexAttribPointer(unsigned(position->attributeID),3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);glEnableVertexAttribArray(unsigned(position->attributeID));
        glVertexAttribPointer(unsigned(colour->attributeID),3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(sizeof(float)*3));glEnableVertexAttribArray(unsigned(colour->attributeID));
        // Grid first: transparent older data can reveal the true coordinate plane.
        vertices.clear();auto add=[&](float x,float y,float z){vertices.push_back({x,y,z,a.grid.getFloatRed()*1.5f,a.grid.getFloatGreen()*1.5f,a.grid.getFloatBlue()*1.5f});};
        for(int i=0;i<=8;++i){float bound=xyCapture?.4761904762f:1.f;float x=-bound+i*bound*.25f,z=i/8.f;add(x,xyCapture?.4f:0.f,0);add(x,xyCapture?.4f:0.f,1);add(-bound,xyCapture?.4f:0.f,z);add(bound,xyCapture?.4f:0.f,z);}
        if(xyCapture){add(0,-.2666667f,0);add(0,1.0666667f,0);}else{add(-1,0,0);add(-1,1,0);}
        if(!xyCapture&&(low>20.1f||high<frame->maxFrequency-.1f)) {auto marker=[&](float x,float y,float z){vertices.push_back({x,y,z,a.right.getFloatRed(),a.right.getFloatGreen(),a.right.getFloatBlue()});};float x0=std::clamp(spectrumX(low,logarithmic,frame->maxFrequency)*2-1,-1.f,1.f),x1=std::clamp(spectrumX(high,logarithmic,frame->maxFrequency)*2-1,-1.f,1.f);for(float x:{x0,x1}){marker(x,0,0);marker(x,1,0);marker(x,0,0);marker(x,0,1);}marker(x0,1,0);marker(x1,1,0);}
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(Vertex)),vertices.data(),GL_DYNAMIC_DRAW);surface->set(0.f);if(value("clean")<.5f)glDrawArrays(GL_LINES,0,int(vertices.size()));
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        if(xyCapture) {
            glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,waveTexture);
            if(trajectory!=uploadedTrajectory){int rows=int(trajectory->data.size())/TrajectoryFrame::textureWidth;glPixelStorei(GL_UNPACK_ALIGNMENT,4);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);if(rows!=waveRows){glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,TrajectoryFrame::textureWidth,rows,0,GL_RGBA,GL_FLOAT,trajectory->data.data());waveRows=rows;}else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,TrajectoryFrame::textureWidth,rows,GL_RGBA,GL_FLOAT,trajectory->data.data());uploadedTrajectory=trajectory;}
            const double frequency=value("xyFreq"),delay=value("xyDelay")*.001*trajectory->rate;
            const float phase=float(std::remainder((trajectory->base-delay)*frequency/trajectory->rate+value("xyPhase")/360.,1.));
            lineShader->use();lineWave->set(1);lineCamera->set(angleYaw()*juce::MathConstants<float>::pi/180,anglePitch()*juce::MathConstants<float>::pi/180,float(width)/std::max(1,height),value(zoomID()));
            lineInfo->set(float(trajectory->first),float(trajectory->points),float(trajectory->requested),value("gain"));lineY->set(value("xySource"),float(delay),float(frequency/trajectory->rate),phase);lineOsc->set(value("xyAmp"),float(trajectory->count),value("xyFollow"));lineStroke->set(float(width),float(height),value("lineWidth"));
            lineColour->set(a.left.getFloatRed(),a.left.getFloatGreen(),a.left.getFloatBlue());lineFade->set(value("rearFade")*.01f);
            glDisableVertexAttribArray(unsigned(position->attributeID));glDisableVertexAttribArray(unsigned(colour->attributeID));
            glDrawArrays(GL_LINE_STRIP,0,trajectory->points);
        } else {
            vertices.resize(size_t(frame->rowCount)*SpectrumFrame::columns);const float weighting=tilt(),floorDb=value("fftFloor"),range=ceiling()-floorDb;const bool isListening=listening();
            std::array<float,SpectrumFrame::columns> frequencies{},x{},offset{};for(int c=0;c<SpectrumFrame::columns;++c){float f=spectrumFrequency(float(c)/(SpectrumFrame::columns-1),true,frame->maxFrequency);frequencies[size_t(c)]=f;x[size_t(c)]=spectrumX(f,logarithmic,frame->maxFrequency)*2-1;offset[size_t(c)]=value("fftGain")+weighting*std::log2(f/1000.f);}
            struct RGB{float r,g,b;};std::array<RGB,256> palette;for(int i=0;i<256;++i){float h=i/255.f;auto col=a.panel.interpolatedWith(a.left,std::min(1.f,.18f+h*.95f));if(h>.42f&&int(value("style"))!=3)col=col.interpolatedWith(a.right,std::min(1.f,(h-.42f)/.42f));palette[size_t(i)]={col.getFloatRed(),col.getFloatGreen(),col.getFloatBlue()};}
            for(int r=0;r<frame->rowCount;++r)for(int c=0;c<SpectrumFrame::columns;++c){float h=std::clamp((frame->history[size_t(r*SpectrumFrame::columns+c)]+offset[size_t(c)]-floorDb)/range,0.f,1.f);auto col=palette[size_t(juce::roundToInt(h*255))];if(isListening&&(frequencies[size_t(c)]<low||frequencies[size_t(c)]>high)){col.r*=.25f;col.g*=.25f;col.b*=.25f;}vertices[size_t(r*SpectrumFrame::columns+c)]={x[size_t(c)],h,frame->ages[size_t(r)],col.r,col.g,col.b};}
            glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(Vertex)),vertices.data(),GL_DYNAMIC_DRAW);const bool wire=value("fftWireframe")>.5f;surface->set(wire?3.f:1.f);if(wire)glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);const float yr=angleYaw()*juce::MathConstants<float>::pi/180,pr=anglePitch()*juce::MathConstants<float>::pi/180;
            const int order=(std::cos(yr)*std::cos(pr)<0?1:0)|(-std::sin(yr)*std::cos(pr)<0?2:0);
            const size_t firstRow=size_t(order)*(SpectrumFrame::rows-1)+((order&1)?0:size_t(SpectrumFrame::rows-frame->rowCount));
            if(frame->rowCount>1)glDrawElements(GL_TRIANGLES,(frame->rowCount-1)*(SpectrumFrame::columns-1)*6,GL_UNSIGNED_INT,reinterpret_cast<const void*>(firstRow*(SpectrumFrame::columns-1)*6*sizeof(unsigned)));
            if(wire)glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        }
        glDisable(GL_BLEND);glActiveTexture(GL_TEXTURE0);glBindVertexArray(0);glDisable(GL_DEPTH_TEST);
        auto overlay=std::atomic_load(&overlayImage);
        if(overlay&&value("clean")<.5f){if(overlay!=uploadedOverlay){overlayTexture.loadImage(*overlay);uploadedOverlay=overlay;}glActiveTexture(GL_TEXTURE0);overlayTexture.bind();const int w=juce::roundToInt(canvasWidth.load()*context.getRenderingScale()),h=juce::roundToInt(canvasHeight.load()*context.getRenderingScale());context.copyTexture({0,0,w,h},{0,0,w,h},w,h,false);overlayTexture.unbind();}
        ++renderCount;unsigned seq=xyCapture?unsigned(trajectory->position):frame->sequence;if(seq!=renderedSequence){renderedSequence=seq;++updatedCount;}
        if(captureRequested.exchange(false)) {
            const int w=juce::roundToInt(canvasWidth.load()*context.getRenderingScale()),h=juce::roundToInt(canvasHeight.load()*context.getRenderingScale());
            std::vector<juce::uint8> rgba(size_t(w*h*4));glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
            auto image=std::make_shared<juce::Image>(juce::Image::ARGB,w,h,true,juce::SoftwareImageType{});
            juce::Image::BitmapData pixels(*image,juce::Image::BitmapData::writeOnly);
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto* p=&rgba[size_t(((h-1-y)*w+x)*4)];reinterpret_cast<juce::PixelARGB*>(pixels.getLinePointer(y))[x]=juce::PixelARGB(255,p[0],p[1],p[2]);}
            std::atomic_store(&captured,std::shared_ptr<const juce::Image>(std::move(image)));
        }
    }
public:
    std::function<void()> expand;
    SpectrumPanel(SpectrumAnalyser& a,juce::AudioProcessorValueTreeState& s,Capture* c=nullptr,std::atomic<const void*>* preview=nullptr):analyser(a),xyCapture(c),state(s),previewOwner(preview) {
        setOpaque(true);setWantsKeyboardFocus(true);context.setRenderer(this);context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
        context.setComponentPaintingEnabled(false);context.setContinuousRepainting(false);
    }
    ~SpectrumPanel() override {endBandGesture();context.detach();context.setRenderer(nullptr);}
    void set3D(bool enabled) {if(enabled==threeD)return;threeD=enabled;overlayKey.fill(-1);failed.store(false);if(threeD)context.attachTo(*this);else context.detach();repaint();}
    bool tick() {
        if(xyCapture&&xyCapture->position>=2){auto previous=std::atomic_load(&trajectoryFrame);double seconds=value("xy3DTime");if(!previous||previous->position!=xyCapture->position||previous->seconds!=seconds)std::atomic_store(&trajectoryFrame,trajectory(*xyCapture,seconds));}
        auto f=analyser.snapshot();auto xy=std::atomic_load(&trajectoryFrame);bool fresh=xyCapture?xy&&unsigned(xy->position)!=lastSequence:f&&f->sequence!=lastSequence;
        if(xyCapture&&xy)lastSequence=unsigned(xy->position);if(f&&!xyCapture)lastSequence=f->sequence;
        if(threeD){
            std::array<float,19> key{{float(getWidth()),float(getHeight()),angleYaw(),anglePitch(),value("fftScale"),value("fftWeight"),xyCapture?value("gain"):value("fftTilt"),value("fftFloor"),value("history"),value("style"),value("colourL"),value("colourR"),ready.load()?1.f:0.f,xyCapture?(xy?1.f:0.f):(f?1.f:0.f),xyCapture?value("xy3DTime"):(f?f->maxFrequency:20000),expand?1.f:0.f,value("fftGain"),value(zoomID()),value("clean")}};
            if(key!=overlayKey && getWidth()>0 && getHeight()>0){overlayKey=key;const float scale=std::clamp(float(getPeer()?getPeer()->getPlatformScaleFactor():1.),1.f,2.f);auto image=std::make_shared<juce::Image>(juce::Image::ARGB,juce::roundToInt(getWidth()*scale),juce::roundToInt(getHeight()*scale),true,juce::SoftwareImageType{});juce::Graphics g(*image);g.addTransform(juce::AffineTransform::scale(scale));paint(g);std::atomic_store(&overlayImage,std::shared_ptr<const juce::Image>(std::move(image)));}
            context.triggerRepaint();
        }else repaint();
        return fresh;
    }
    unsigned takePaints() {return renderCount.exchange(0);}
    unsigned takeUpdates() {return updatedCount.exchange(0);}
    void requestCapture() {captureRequested.store(true);context.triggerRepaint();}
    std::shared_ptr<const juce::Image> capturedImage() const {return std::atomic_load(&captured);}
    bool graphicsReady() const {return ready.load()&&!failed.load();}
    void resized() override {canvasWidth.store(getWidth());canvasHeight.store(getHeight());}
    void paint(juce::Graphics& g) override {
        auto a=theme();auto frame=analyser.snapshot();g.setFont(11.f);
        const bool log=value("fftScale")<.5f;
        const float max=frame?frame->maxFrequency:20000;
        const float floorDb=value("fftFloor"),topDb=ceiling(),weighting=tilt();
        const auto box=juce::Rectangle<float>(48,25,float(getWidth()-68),float(getHeight()-66));
        auto fx=[&](float f){return box.getX()+box.getWidth()*spectrumX(f,log,max);};
        auto fy=[&](float db){return box.getBottom()-box.getHeight()*(db-floorDb)/(topDb-floorDb);};
        if(!threeD) {
            ++renderCount;if(frame&&frame->sequence!=renderedSequence){renderedSequence=frame->sequence;++updatedCount;}g.fillAll(a.panel);
            if(value("clean")<.5f)for(float db=std::ceil(value("fftFloor")/12)*12;db<=ceiling();db+=12) {g.setColour(a.grid);g.drawHorizontalLine(int(fy(db)),box.getX(),box.getRight());g.setColour(a.muted);g.drawText(juce::String(int(db)),0,int(fy(db))-7,42,14,juce::Justification::centredRight);}
            const std::vector<float> ticks=log?std::vector<float>{20,50,100,200,500,1000,2000,5000,10000,20000}:std::vector<float>{0,2000,4000,6000,8000,10000,12000,14000,16000,18000,20000};
            if(value("clean")<.5f)for(float f:ticks)if(f<=max) {g.setColour(a.grid);g.drawVerticalLine(int(fx(f)),box.getY(),box.getBottom());g.setColour(a.muted);g.drawText(hz(f),int(fx(f))-22,int(box.getBottom())+5,44,16,juce::Justification::centred);}
            if(frame) {
                juce::Path p,peak;
                for(int i=0;i<SpectrumFrame::bands;++i) {float f=spectrumFrequency(float(i)/(SpectrumFrame::bands-1),true,max);float d=frame->db[size_t(i)]+value("fftGain")+weighting*std::log2(f/1000.f);float pk=frame->peak[size_t(i)]+value("fftGain")+weighting*std::log2(f/1000.f);float x=fx(f),y=std::clamp(fy(d),box.getY(),box.getBottom());
                    if(i==0) {p.startNewSubPath(x,y);peak.startNewSubPath(x,std::clamp(fy(pk),box.getY(),box.getBottom()));}else {p.lineTo(x,y);peak.lineTo(x,std::clamp(fy(pk),box.getY(),box.getBottom()));}}
                auto fill=p;fill.lineTo(box.getRight(),box.getBottom());fill.lineTo(box.getX(),box.getBottom());fill.closeSubPath();
                g.setGradientFill(juce::ColourGradient(a.left.withAlpha(.24f),0,box.getY(),a.left.withAlpha(.015f),0,box.getBottom(),false));g.fillPath(fill);
                g.setColour(a.right.withAlpha(.4f));g.strokePath(peak,juce::PathStrokeType(.8f));g.setColour(a.left);g.strokePath(p,juce::PathStrokeType(value("lineWidth"),juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
            }
            float lo=std::clamp(fx(std::min(value("bandLow"),value("bandHigh"))),box.getX(),box.getRight()),hi=std::clamp(fx(std::max(value("bandLow"),value("bandHigh"))),lo,box.getRight());
            if(value("clean")<.5f&&(value("bandLow")>20.1f||value("bandHigh")<max-.1f||listening())) {
                if(listening()) {g.setColour(a.panel.withAlpha(.68f));g.fillRect(box.withRight(lo));g.fillRect(box.withX(hi));}
                g.setColour(a.right.withAlpha(.13f));g.fillRect(juce::Rectangle<float>(lo,box.getY(),hi-lo,box.getHeight()));g.setColour(a.right);g.drawVerticalLine(int(lo),box.getY(),box.getBottom());g.drawVerticalLine(int(hi),box.getY(),box.getBottom());
                for(float edge:{lo,hi})g.fillRoundedRectangle(edge-3,box.getCentreY()-12,6,24,3);
                auto grip=bandGrip();g.setColour(a.panel.withAlpha(.9f));g.fillRoundedRectangle(grip,5);
                g.setColour(a.left.withAlpha(selecting&&bandDrag==BandDrag::move?1.f:.8f));g.drawRoundedRectangle(grip,5,1);
                auto centre=grip.getCentre();
                g.drawArrow({centre,{grip.getX()+5,centre.y}},1.3f,4,4);
                g.drawArrow({centre,{grip.getRight()-5,centre.y}},1.3f,4,4);
            }
            if(value("clean")<.5f && hover>=0 && frame) {float f=std::max(20.f,hover);float db=spectrumValue(frame->db,f,max)+value("fftGain")+weighting*std::log2(f/1000);int note=juce::roundToInt(69+12*std::log2(f/440));
                g.setColour(a.text);g.drawText(hz(f)+" Hz / "+juce::String(db,1)+" dB / "+juce::MidiMessage::getMidiNoteName(note,true,true,4),int(box.getX())+8,int(box.getY())+5,350,18,juce::Justification::centredLeft);}
        } else {
            if(failed.load()) {g.fillAll(a.panel);g.setColour(a.text);g.drawText("3D renderer unavailable",getLocalBounds(),juce::Justification::centred);return;}
            if(!ready.load()) {g.fillAll(a.panel);g.setColour(a.text);g.drawText("Starting 3D renderer...",getLocalBounds(),juce::Justification::centred);return;}
            if(value("clean")>.5f)return;
            if(xyCapture){
                g.setColour(a.muted);for(float v:{-1.f,0.f,1.f}){auto x=project(v*.4761904762f,.4f,0),y=project(0,.4f+v*.6666666667f,0);g.drawText("X "+juce::String(v/value("gain"),2),int(x.x)-22,int(x.y)+5,44,16,juce::Justification::centred);if(v!=0){auto bounds=juce::Rectangle<int>(int(y.x)+6,int(y.y)-7,64,16);g.setColour(a.panel.withAlpha(.8f));g.fillRoundedRectangle(bounds.toFloat(),3);g.setColour(a.muted);g.drawText("Y "+juce::String(v/value("gain"),2),bounds,juce::Justification::centredLeft);}}
                for(int i=0;i<=4;++i){float age=i/4.f;auto p=project(.4761904762f,.4f,age);g.drawText(i==0?"now":"-"+juce::String(value("xy3DTime")*age,2)+" s",int(p.x)+6,int(p.y)-7,70,16,juce::Justification::centredLeft);}
                g.drawText("X: Left / Y: selected source / Z: time",10,4,getWidth()-20,18,juce::Justification::centredLeft);g.drawText("Drag: rotate / wheel: zoom / double-click: reset",8,getHeight()-21,getWidth()-16,18,juce::Justification::centredLeft);return;
            }
            g.setColour(a.muted);
            for(float f:(log?std::vector<float>{20,100,1000,10000,20000}:std::vector<float>{0,5000,10000,15000,20000}))if(f<=max) {
                auto p=project(spectrumX(f,log,max)*2-1,0,0);g.drawText(hz(f),int(p.x)-22,int(p.y)+5,44,16,juce::Justification::centred);
            }
            for(int i=0;i<=4;++i) {float age=i/4.f;auto p=project(1,0,age);g.drawText(i==0?"now":"-"+juce::String(value("history")*age,1)+" s",int(p.x)+6,int(p.y)-7,62,16,juce::Justification::centredLeft);}
            for(int i=0;i<=3;++i) {float h=i/3.f;auto p=project(-1,h,0);g.drawText(juce::String(juce::roundToInt(value("fftFloor")+h*(ceiling()-value("fftFloor")))),int(p.x)-40,int(p.y)-7,34,16,juce::Justification::centredRight);}
        }
        if(value("clean")>.5f)return;
        g.setColour(a.muted);g.drawText("dBFS"+juce::String(tilt()==0?"":" + "+juce::String(tilt(),1)+" dB/oct @ 1k")+(value("fftGain")==0?juce::String():" / visual gain "+juce::String(value("fftGain")>0?"+":"")+juce::String(value("fftGain"),0)+" dB"),10,4,440,18,juce::Justification::centredLeft);
        g.drawText(threeD?"Drag: rotate / wheel: zoom / Shift-drag: band / double-click: reset view":"Drag: select / grip: sweep / Alt+grip: preview / edges: resize / Esc: original",8,getHeight()-21,getWidth()-16,18,juce::Justification::centredLeft);
        if(!frame) {g.setColour(a.text);g.drawText("Waiting for audio...",getLocalBounds(),juce::Justification::centred);}
    }
    void mouseMove(const juce::MouseEvent& e) override {hover=mouseFrequency(e.position);bandCursor(e.position);if(!threeD)repaint();}
    void mouseExit(const juce::MouseEvent&) override {hover=-1;if(!selecting)setMouseCursor(juce::MouseCursor::NormalCursor);repaint();}
    void mouseDown(const juce::MouseEvent& e) override {
        grabKeyboardFocus();
        if(expand && (threeD||e.position.y<24) && (e.position.y<24 || e.getNumberOfClicks()>1)){selecting=rotating=false;return;}
        dragStart=e.position;rotating=threeD&&(xyCapture||!e.mods.isShiftDown());selecting=!rotating;
        if(rotating) {startYaw=angleYaw();startPitch=anglePitch();for(auto* id:{yawID(),pitchID()})state.getParameter(id)->beginChangeGesture();}
        else {
            bandDrag=bandHit(e.position);dragLogarithmic=value("fftScale")<.5f;
            auto initial=shiftBand(value("bandLow"),value("bandHigh"),0,dragLogarithmic,maxFrequency());startBandLow=initial[0];startBandHigh=initial[1];
            for(auto* id:{"bandLow","bandHigh"})state.getParameter(id)->beginChangeGesture();
            if(bandDrag==BandDrag::create) {
                anchor=mouseFrequency(e.position);float low=std::clamp(anchor*.9f,20.f,maxFrequency()-1);
                set("bandLow",low);set("bandHigh",std::clamp(anchor*1.1f,low+1,maxFrequency()));
            }
            temporaryPreview(bandDrag==BandDrag::move&&e.mods.isAltDown());
            bandCursor(e.position);repaint();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        temporaryPreview(selecting&&bandDrag==BandDrag::move&&e.mods.isAltDown());
        if(rotating) {set(yawID(),std::remainder(startYaw+(e.position.x-dragStart.x)*.25f-value(baseYawID()),360.f));set(pitchID(),std::remainder(startPitch-(e.position.y-dragStart.y)*.25f-value(basePitchID()),360.f));repaint();context.triggerRepaint();}
        else if(selecting)select(e.position);
    }
    void mouseUp(const juce::MouseEvent& e) override {endBandGesture();bandCursor(e.position);repaint();}
    void modifierKeysChanged(const juce::ModifierKeys& mods) override {
        temporaryPreview(selecting&&bandDrag==BandDrag::move&&mods.isAltDown());repaint();
        juce::Component::modifierKeysChanged(mods);
    }
    void focusLost(juce::Component::FocusChangeType) override {endBandGesture();}
    void visibilityChanged() override {if(!isVisible())endBandGesture();}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel)override {
        if(!threeD){juce::Component::mouseWheelMove(e,wheel);return;}
        if(wheel.deltaY==0)return;auto* p=state.getParameter(zoomID());p->beginChangeGesture();
        set(zoomID(),std::clamp(value(zoomID())*std::exp(wheel.deltaY*1.5f),.5f,3.f));p->endChangeGesture();tick();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override {if(expand&&(threeD||getMouseXYRelative().y<24)){expand();return;}if(threeD) {set(baseYawID(),-14);set(basePitchID(),35);set(yawID(),0);set(pitchID(),0);set(zoomID(),1);}else {set("listen",0);set("bandLow",20);set("bandHigh",20000);}repaint();context.triggerRepaint();}
    bool keyPressed(const juce::KeyPress& key) override {if(key==juce::KeyPress::escapeKey){endBandGesture();set("listen",0);return false;}return false;}
};
}
