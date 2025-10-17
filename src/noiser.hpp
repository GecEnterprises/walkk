// imgui_noise_embed.h  (single file, just include once in a .cpp)
// Requires: OpenGL 3.3+, Dear ImGui already initialized with OpenGL backend.

#pragma once

#include "imgui.h"

// ===== Choose your GL loader (or include your own before this file) =====
#if !defined(IMGUI_NOISE_USE_GLAD) && !defined(IMGUI_NOISE_USE_GL3W)
  // Default to glad
  #define IMGUI_NOISE_USE_GLAD
#endif

#ifdef IMGUI_NOISE_USE_GLAD
  #include <glad/gl.h>
#elif defined(IMGUI_NOISE_USE_GL3W)
  #include <GL/gl3w.h>
#else
  #error "Define IMGUI_NOISE_USE_GLAD or IMGUI_NOISE_USE_GL3W or include a GL 3.3 loader before this header."
#endif

#include <stdint.h>
#include <stdio.h>

// ===================== Global Configuration =====================
// Controls how many spiral rotations before wrapping (lower = tighter spiral, more frequent)
static const float IMGUI_NOISE_SPIRAL_WRAP_CYCLES = 3.0f;

// ===================== Internal (static) =====================
namespace {
    struct ImGuiNoise__FBO { GLuint fbo=0, color=0; int w=0,h=0; };
    static ImGuiNoise__FBO    g_noise_fbo;
    static GLuint             g_noise_prog = 0;
    static GLuint             g_noise_vao  = 0, g_noise_vbo = 0;
    static GLint              g_uTime=-1, g_uScale=-1, g_uSpeed=-1, g_uRes=-1;
    static GLint              g_uBgColor=-1, g_uLineColor=-1, g_uWrapCycles=-1;
    static float              g_noise_scale = 6.0f;
    static float              g_noise_speed = 1.0f;
    static double             g_noise_time  = 0.0;  // seconds, accumulated
    static bool               g_noise_inited = false;
    static ImVec4             g_bg_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    static ImVec4             g_line_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    static const float FSQ[] = {
        // pos     // uv
        -1.f,-1.f, 0.f,0.f,
         1.f,-1.f, 1.f,0.f,
         1.f, 1.f, 1.f,1.f,
        -1.f,-1.f, 0.f,0.f,
         1.f, 1.f, 1.f,1.f,
        -1.f, 1.f, 0.f,1.f,
    };

    static const char* NOISE_VS = R"GLSL(
        #version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aUV;
        out vec2 vUV;
        void main(){
            vUV = aUV;
            gl_Position = vec4(aPos,0.0,1.0);
        }
    )GLSL";

    static const char* NOISE_FS = R"GLSL(
        #version 330 core
        out vec4 FragColor;
        in vec2 vUV;
        uniform float uTime;
        uniform float uScale;
        uniform float uSpeed;
        uniform vec2  uResolution;
        uniform vec3  uBgColor;
        uniform vec3  uLineColor;
        uniform float uWrapCycles;

        float hash(vec2 p){
            return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
        }

        float valueNoise(vec2 p){
            vec2 i=floor(p), f=fract(p);
            float a=hash(i+vec2(0,0));
            float b=hash(i+vec2(1,0));
            float c=hash(i+vec2(0,1));
            float d=hash(i+vec2(1,1));
            vec2 u = f*f*(3.0-2.0*f);
            return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
        }

      // --- inside NOISE_FS ---

void main(){
    vec2 uv = (gl_FragCoord.xy / uResolution.xy) * 2.0 - 1.0;
    uv.x *= uResolution.x / uResolution.y;

    float dist = length(uv);
    float angle = atan(uv.y, uv.x);

    const float PI = 3.14159265359;
    const float TAU = PI * 2.0;

    // 1) Spin (what you already had)
    float rotationSpeed = uTime * uSpeed * 0.3;

    // 2) Spiral tightness (you can tie this to uScale if you want)
    float spiralTightness = 0.2;

    // 3) This is the key: use time to push the pattern inward.
    //    Increasing 'inward' over time decreases expectedTheta,
    //    which moves the match toward smaller radii.
    float inward = uTime * uSpeed * 0.5;   // tweak 0.5 as you like

    float logDist   = log(max(dist, 0.001));
    float baseAngle = angle - rotationSpeed;

    // normalize to [-PI, PI]
    baseAngle = mod(baseAngle + PI, TAU) - PI;

    // BEFORE: float expectedTheta = (logDist + 2.0) / spiralTightness;
    // AFTER: subtract 'inward' to march inward over time
    float expectedTheta = (logDist + 2.0 - inward) / spiralTightness;

    float thetaDiff     = baseAngle - expectedTheta;
    float wrappedTheta  = mod(thetaDiff + PI, TAU * uWrapCycles) - PI;
    float spiralDist    = abs(wrappedTheta) * dist;

    float lineWidth = 2.0 / uResolution.y;
    float lineAlpha = smoothstep(lineWidth * 2.0, lineWidth * 0.5, spiralDist);

    float noiseVal = valueNoise(vec2(angle * 8.0 + uTime * 0.5, logDist * 2.0)) * 0.5 + 0.5;
    lineAlpha *= mix(0.7, 1.0, noiseVal);

    float fadeMask = smoothstep(1.2, 0.9, dist) * smoothstep(0.01, 0.05, dist);
    lineAlpha *= fadeMask;

    // Extra arms (unchanged)
    for(int i = 1; i < 3; i++){
        float armOffset = float(i) * TAU / 3.0;
        float armAngle = baseAngle + armOffset;
        armAngle = mod(armAngle + PI, TAU) - PI;

        float armThetaDiff = armAngle - expectedTheta;
        float armWrappedTheta = mod(armThetaDiff + PI, TAU * uWrapCycles) - PI;
        float armSpiralDist = abs(armWrappedTheta) * dist;

        float armAlpha = smoothstep(lineWidth * 2.0, lineWidth * 0.5, armSpiralDist);
        float armNoise = valueNoise(vec2(armAngle * 8.0 + uTime * 0.5, logDist * 2.0 + float(i))) * 0.5 + 0.5;
        armAlpha *= mix(0.7, 1.0, armNoise);
        armAlpha *= fadeMask;

        lineAlpha = max(lineAlpha, armAlpha);
    }

    vec3 col = mix(uBgColor, uLineColor, lineAlpha);
    FragColor = vec4(col, 1.0);
}

    )GLSL";

    static GLuint compileShader(GLenum type, const char* src){
        GLuint s = glCreateShader(type);
        glShaderSource(s,1,&src,nullptr);
        glCompileShader(s);
        GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if(!ok){
            GLint len=0; glGetShaderiv(s,GL_INFO_LOG_LENGTH,&len);
            ImVector<char> log; log.resize(len+1);
            glGetShaderInfoLog(s,len,nullptr,log.Data);
            fprintf(stderr,"[ImGuiNoise] Shader compile error:\n%s\n", log.Data);
            glDeleteShader(s);
            return 0;
        }
        return s;
    }
    static GLuint linkProg(GLuint vs, GLuint fs){
        GLuint p = glCreateProgram();
        glAttachShader(p,vs); glAttachShader(p,fs);
        glLinkProgram(p);
        GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
        if(!ok){
            GLint len=0; glGetProgramiv(p,GL_INFO_LOG_LENGTH,&len);
            ImVector<char> log; log.resize(len+1);
            glGetProgramInfoLog(p,len,nullptr,log.Data);
            fprintf(stderr,"[ImGuiNoise] Program link error:\n%s\n", log.Data);
            glDeleteProgram(p);
            return 0;
        }
        return p;
    }
    static void destroyFBO(ImGuiNoise__FBO& f){
        if(f.color){ glDeleteTextures(1,&f.color); f.color=0; }
        if(f.fbo){ glDeleteFramebuffers(1,&f.fbo); f.fbo=0; }
        f.w=f.h=0;
    }
    static bool createFBO(ImGuiNoise__FBO& f, int w, int h){
        if(w<=0||h<=0) return false;
        destroyFBO(f);
        f.w=w; f.h=h;
        glGenTextures(1,&f.color);
        glBindTexture(GL_TEXTURE_2D,f.color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glGenFramebuffers(1,&f.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER,f.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,f.color,0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        if(status!=GL_FRAMEBUFFER_COMPLETE){
            fprintf(stderr,"[ImGuiNoise] FBO incomplete: 0x%x\n", status);
            destroyFBO(f);
            return false;
        }
        return true;
    }
    static bool ensureGLResources(int w, int h){
        if(!g_noise_prog){
            GLuint vs=compileShader(GL_VERTEX_SHADER, NOISE_VS);
            GLuint fs=compileShader(GL_FRAGMENT_SHADER, NOISE_FS);
            if(!vs || !fs) return false;
            g_noise_prog = linkProg(vs,fs);
            glDeleteShader(vs); glDeleteShader(fs);
            if(!g_noise_prog) return false;
            g_uTime  = glGetUniformLocation(g_noise_prog,"uTime");
            g_uScale = glGetUniformLocation(g_noise_prog,"uScale");
            g_uSpeed = glGetUniformLocation(g_noise_prog,"uSpeed");
            g_uRes   = glGetUniformLocation(g_noise_prog,"uResolution");
            g_uBgColor = glGetUniformLocation(g_noise_prog,"uBgColor");
            g_uLineColor = glGetUniformLocation(g_noise_prog,"uLineColor");
            g_uWrapCycles = glGetUniformLocation(g_noise_prog,"uWrapCycles");
            glGenVertexArrays(1,&g_noise_vao);
            glGenBuffers(1,&g_noise_vbo);
            glBindVertexArray(g_noise_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_noise_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(FSQ), FSQ, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
            glBindVertexArray(0);
        }
        if(!g_noise_fbo.fbo || g_noise_fbo.w!=w || g_noise_fbo.h!=h){
            if(!createFBO(g_noise_fbo,w,h)) return false;
        }
        return true;
    }
    static void renderToFBO(){
        if(!g_noise_fbo.fbo) return;
        GLint prevFBO=0; glGetIntegerv(GL_FRAMEBUFFER_BINDING,&prevFBO);
        GLint prevVAO=0; glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&prevVAO);
        GLint prevProg=0; glGetIntegerv(GL_CURRENT_PROGRAM,&prevProg);
        GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp);

        glBindFramebuffer(GL_FRAMEBUFFER, g_noise_fbo.fbo);
        glViewport(0,0,g_noise_fbo.w,g_noise_fbo.h);
        glDisable(GL_DEPTH_TEST);
        glClearColor(g_bg_color.x, g_bg_color.y, g_bg_color.z, g_bg_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(g_noise_prog);
        glUniform1f(g_uTime,  (float)g_noise_time);
        glUniform1f(g_uScale, (float)g_noise_scale);
        glUniform1f(g_uSpeed, (float)g_noise_speed);
        glUniform2f(g_uRes,   (float)g_noise_fbo.w, (float)g_noise_fbo.h);
        glUniform3f(g_uBgColor, g_bg_color.x, g_bg_color.y, g_bg_color.z);
        glUniform3f(g_uLineColor, g_line_color.x, g_line_color.y, g_line_color.z);
        glUniform1f(g_uWrapCycles, IMGUI_NOISE_SPIRAL_WRAP_CYCLES);

        glBindVertexArray(g_noise_vao);
        glDrawArrays(GL_TRIANGLES,0,6);

        // restore
        glBindVertexArray(prevVAO);
        glUseProgram(prevProg);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(vp[0],vp[1],vp[2],vp[3]);
    }
} // anon

// ===================== Public API (no structs) =====================

// Call once after your GL context & ImGui are ready.
// 'w','h' are initial offscreen resolution (use your framebuffer size).
// 'bg_color' and 'line_color' set the background and spiral line colors.
inline bool ImGuiNoise_InitOnce(int w, int h, ImVec4 bg_color = ImVec4(0,0,0,1), ImVec4 line_color = ImVec4(1,1,1,1)){
    if(g_noise_inited) return true;
    g_bg_color = bg_color;
    g_line_color = line_color;
    g_noise_inited = ensureGLResources(w,h);
    return g_noise_inited;
}

// If your framebuffer resizes (or you want a different render res), call this.
inline bool ImGuiNoise_Resize(int w, int h){
    if(!g_noise_inited) return ImGuiNoise_InitOnce(w,h);
    return ensureGLResources(w,h);
}

// Optional setters if you want to drive parameters from outside.
inline void ImGuiNoise_SetScale(float scale){ g_noise_scale = scale; }
inline void ImGuiNoise_SetSpeed(float speed){ g_noise_speed = speed; }
inline void ImGuiNoise_AddTime(double dt){ g_noise_time += dt; }  // accumulate yourself
inline void ImGuiNoise_SetTime(double t){ g_noise_time = t; }


static void ImDrawCallback_SetDifference(const ImDrawList*, const ImDrawCmd*)
{
    // Save whatever you need on your side if necessary
    glEnable(GL_BLEND);
    // Reverse subtract approximates difference (NOT abs)
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE); // src - dst (clamped)
}

static void ImDrawCallback_ResetBlend(const ImDrawList*, const ImDrawCmd*)
{
    // Restore default ImGui blend
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


// Convenience: draw a ready-to-use widget that renders & shows the preview + controls.
// - label: panel title
// - size: requested preview size (0,0 = use available content region)
// Returns true if displayed.
inline bool ImGuiNoise_Draw(const char* label = "Noise Preview",
                            ImVec2 size = ImVec2(0,0),
                            bool show_controls = true)
{
    if(!g_noise_inited) return false;

    // 1) Render the noise into the FBO
    renderToFBO();

    if(show_controls){
        ImGui::SliderFloat("Scale (freq)", &g_noise_scale, 0.5f, 20.0f, "%.2f");
        ImGui::SliderFloat("Speed", &g_noise_speed, 0.0f, 5.0f, "%.2f");
        ImGui::Text("FBO: %dx%d  Time: %.2fs", g_noise_fbo.w, g_noise_fbo.h, (float)g_noise_time);
        ImGui::Separator();
    }

    if(size.x <= 0 || size.y <= 0)
        size = ImGui::GetContentRegionAvail();

    // Important: flip V (OpenGL origin bottom-left vs ImGui top-left)
    ImTextureID id = (ImTextureID)(intptr_t)g_noise_fbo.color;

ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Center in the main viewport (works even if there’s no window open/focused)
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 vp_pos  = vp->Pos;
    ImVec2 vp_size = vp->Size;

ImVec2 pmin = ImVec2(
    vp_pos.x + (vp_size.x - size.x) * 0.5f,
    vp_pos.y + (vp_size.y - size.y) * 0.5f
);

ImVec2 pmax = ImVec2(
    pmin.x + size.x,
    pmin.y + size.y
);


    dl->AddCallback(ImDrawCallback_SetDifference, nullptr); 

    // Optional: pass a tint (IM_COL32_WHITE = unchanged)
    dl->AddImage(id, pmin, pmax, ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE);

    // ImGui::Image(id, size, ImVec2(0,1), ImVec2(1,0));

      dl->AddCallback(ImDrawCallback_ResetBlend, nullptr);
    // A "null" callback resets ImGui's internal state tracking; optional for GL
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    return true;
}

// If you only want the texture (e.g., to compose inside your own UI), call this.
// It renders the FBO and returns the ImTextureID you can pass to ImGui::Image yourself.
inline ImTextureID ImGuiNoise_RenderAndGetTexture()
{
    if(!g_noise_inited) return (ImTextureID)(intptr_t)0;
    renderToFBO();
    return (ImTextureID)(intptr_t)g_noise_fbo.color;
}

// Cleanup (optional, call on shutdown).
inline void ImGuiNoise_Shutdown(){
    if(!g_noise_inited) return;
    destroyFBO(g_noise_fbo);
    if(g_noise_vbo){ glDeleteBuffers(1,&g_noise_vbo); g_noise_vbo=0; }
    if(g_noise_vao){ glDeleteVertexArrays(1,&g_noise_vao); g_noise_vao=0; }
    if(g_noise_prog){ glDeleteProgram(g_noise_prog); g_noise_prog=0; }
    g_noise_inited=false;
}