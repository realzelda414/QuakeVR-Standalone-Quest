/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_main.c

#include "quakedef.hpp"
#include "quakedef_macros.hpp"
#include "quakeglm_qvec3.hpp"
#include "server.hpp"
#include "vr.hpp"
#include "vr_showfn.hpp"
#include "vr_cvars.hpp"
#include "util.hpp"
#include "worldtext.hpp"
#include "console.hpp"
#include "sbar.hpp"
#include "glquake.hpp"
#include "shader.hpp"
#include "sys.hpp"
#include "gl_texmgr.hpp"
#include "view.hpp"
#include "vid.hpp"
#include "client.hpp"
#include "q_sound.hpp"
#include "qcvm.hpp"

#include <string_view>
#include <algorithm>

bool r_cache_thrash; // compatability

qvec3 modelorg, r_entorigin;
entity_t* currententity;

int r_visframecount; // bumped when going to a new PVS
int r_framecount;    // used for dlight push checking

mplane_t frustum[4];

// johnfitz -- rendering statistics
int rs_brushpolys, rs_aliaspolys, rs_skypolys, rs_particles, rs_fogpolys;
int rs_dynamiclightmaps, rs_brushpasses, rs_aliaspasses, rs_skypasses;
float rs_megatexels;

//
// view origin
//
qvec3 vup;
qvec3 vpn;
qvec3 vright;
qvec3 r_origin;

float r_fovx, r_fovy; // johnfitz -- rendering fov may be different becuase of
                      // r_waterwarp and r_stereo

//
// screen size info
//
refdef_t r_refdef;

mleaf_t *r_viewleaf, *r_oldviewleaf;

int d_lightstylevalue[MAX_LIGHTSTYLES]; // 8.8 fraction of base light value


cvar_t r_norefresh = {"r_norefresh", "0", CVAR_NONE};
cvar_t r_drawentities = {"r_drawentities", "1", CVAR_NONE};
cvar_t r_drawworldtext = {"r_drawworldtext", "1", CVAR_NONE};
cvar_t r_drawviewmodel = {"r_drawviewmodel", "1", CVAR_NONE};
cvar_t r_speeds = {"r_speeds", "0", CVAR_NONE};
cvar_t r_pos = {"r_pos", "0", CVAR_NONE};
cvar_t r_fullbright = {"r_fullbright", "0", CVAR_NONE};
cvar_t r_lightmap = {"r_lightmap", "0", CVAR_NONE};
cvar_t r_shadows = {"r_shadows", "1", CVAR_ARCHIVE};
cvar_t r_wateralpha = {"r_wateralpha", "1", CVAR_ARCHIVE};
cvar_t r_dynamic = {"r_dynamic", "1", CVAR_ARCHIVE};
cvar_t r_novis = {"r_novis", "0", CVAR_ARCHIVE};

cvar_t gl_finish = {"gl_finish", "0", CVAR_NONE};
cvar_t gl_clear = {"gl_clear", "1", CVAR_NONE};
cvar_t gl_cull = {"gl_cull", "1", CVAR_NONE};
cvar_t gl_smoothmodels = {"gl_smoothmodels", "1", CVAR_NONE};
cvar_t gl_affinemodels = {"gl_affinemodels", "0", CVAR_NONE};
cvar_t gl_polyblend = {"gl_polyblend", "1", CVAR_NONE};
cvar_t gl_flashblend = {"gl_flashblend", "0", CVAR_ARCHIVE};
cvar_t gl_playermip = {"gl_playermip", "0", CVAR_NONE};
cvar_t gl_nocolors = {"gl_nocolors", "0", CVAR_NONE};

// johnfitz -- new cvars
cvar_t r_stereo = {"r_stereo", "0", CVAR_NONE};
cvar_t r_stereodepth = {"r_stereodepth", "128", CVAR_NONE};
cvar_t r_clearcolor = {"r_clearcolor", "2", CVAR_ARCHIVE};
cvar_t r_drawflat = {"r_drawflat", "0", CVAR_NONE};
cvar_t r_flatlightstyles = {"r_flatlightstyles", "0", CVAR_NONE};
cvar_t gl_fullbrights = {"gl_fullbrights", "1", CVAR_ARCHIVE};
cvar_t gl_farclip = {"gl_farclip", "16384", CVAR_ARCHIVE};
cvar_t gl_overbright = {"gl_overbright", "1", CVAR_ARCHIVE};
cvar_t gl_overbright_models = {"gl_overbright_models", "1", CVAR_ARCHIVE};
cvar_t r_oldskyleaf = {"r_oldskyleaf", "0", CVAR_NONE};
cvar_t r_drawworld = {"r_drawworld", "1", CVAR_NONE};
cvar_t r_showtris = {"r_showtris", "0", CVAR_NONE};
cvar_t r_showbboxes = {"r_showbboxes", "0", CVAR_NONE};
cvar_t r_showbboxes_player = {"r_showbboxes_player", "0", CVAR_NONE};
cvar_t r_lerpmodels = {"r_lerpmodels", "1", CVAR_NONE};
cvar_t r_lerpmove = {"r_lerpmove", "1", CVAR_NONE};
cvar_t r_nolerp_list = {"r_nolerp_list",
    "progs/flame.mdl,progs/flame2.mdl,progs/braztall.mdl,progs/"
    "brazshrt.mdl,progs/longtrch.mdl,progs/flame_pyre.mdl,progs/"
    "v_saw.mdl,progs/v_xfist.mdl,progs/h2stuff/newfire.mdl",
    CVAR_NONE};
cvar_t r_noshadow_list = {"r_noshadow_list",
    "progs/flame2.mdl,progs/flame.mdl,progs/bolt1.mdl,progs/bolt2.mdl,progs/"
    "bolt3.mdl,progs/laser.mdl",
    CVAR_NONE};

extern cvar_t r_vfog;
// johnfitz

cvar_t gl_zfix = {"gl_zfix", "0", CVAR_NONE}; // QuakeSpasm z-fighting fix

cvar_t r_lavaalpha = {"r_lavaalpha", "0", CVAR_ARCHIVE};
cvar_t r_telealpha = {"r_telealpha", "0", CVAR_ARCHIVE};
cvar_t r_slimealpha = {"r_slimealpha", "0", CVAR_ARCHIVE};

float map_wateralpha, map_lavaalpha, map_telealpha, map_slimealpha;
float map_fallbackalpha;

bool r_drawflat_cheatsafe, r_fullbright_cheatsafe, r_lightmap_cheatsafe,
    r_drawworld_cheatsafe; // johnfitz

cvar_t r_scale = {"r_scale", "1", CVAR_ARCHIVE};

//==============================================================================
//
// GLSL GAMMA CORRECTION
//
//==============================================================================

static GLuint r_gamma_texture;
static GLuint r_gamma_program;
static int r_gamma_texture_width, r_gamma_texture_height;

// uniforms used in gamma shader
static GLuint gammaLoc;
static GLuint contrastLoc;
static GLuint textureLoc;

/*
=============
GLSLGamma_DeleteTexture
=============
*/
void GLSLGamma_DeleteTexture()
{
    glDeleteTextures(1, &r_gamma_texture);
    r_gamma_texture = 0;
    r_gamma_program = 0;
}

/*
=============
GLSLGamma_CreateShaders
=============
*/
static void GLSLGamma_CreateShaders()
{
    using namespace std::string_view_literals;

    constexpr auto vertSource = R"glsl(
#version 110

void main(void) {
    gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0);
    gl_TexCoord[0] = gl_MultiTexCoord0;
}
)glsl"sv;

    constexpr auto fragSource = R"glsl(
#version 110

uniform sampler2D GammaTexture;
uniform float GammaValue;
uniform float ContrastValue;

void main(void) {
      vec4 frag = texture2D(GammaTexture, gl_TexCoord[0].xy);
      frag.rgb = frag.rgb * ContrastValue;
      gl_FragColor = vec4(pow(frag.rgb, vec3(GammaValue)), 1.0);
}
)glsl"sv;

    if(!gl_glsl_gamma_able)
    {
        return;
    }

    r_gamma_program = quake::make_gl_program(vertSource, fragSource);

    // get uniform locations
    gammaLoc = GL_GetUniformLocation(&r_gamma_program, "GammaValue");
    contrastLoc = GL_GetUniformLocation(&r_gamma_program, "ContrastValue");
    textureLoc = GL_GetUniformLocation(&r_gamma_program, "GammaTexture");
}

/*
=============
GLSLGamma_GammaCorrect
=============
*/
void GLSLGamma_GammaCorrect()
{
    if(!gl_glsl_gamma_able)
    {
        return;
    }

    if(vid_gamma.value == 1 && vid_contrast.value == 1)
    {
        return;
    }

    // create render-to-texture texture if needed
    if(!r_gamma_texture ||
        (r_gamma_texture_width < glwidth || r_gamma_texture_height < glheight))
    {
        if(r_gamma_texture)
        {
            glDeleteTextures(1, &r_gamma_texture);
        }

        glGenTextures(1, &r_gamma_texture);
        glBindTexture(GL_TEXTURE_2D, r_gamma_texture);

        r_gamma_texture_width = glwidth;
        r_gamma_texture_height = glheight;

        if(!gl_texture_NPOT)
        {
            r_gamma_texture_width = TexMgr_Pad(r_gamma_texture_width);
            r_gamma_texture_height = TexMgr_Pad(r_gamma_texture_height);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_gamma_texture_width,
            r_gamma_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    // create shader if needed
    if(!r_gamma_program)
    {
        GLSLGamma_CreateShaders();
        if(!r_gamma_program)
        {
            Sys_Error("GLSLGamma_CreateShaders failed");
        }
    }

    // copy the framebuffer to the texture
    GL_DisableMultitexture();
    glBindTexture(GL_TEXTURE_2D, r_gamma_texture);

    if(vr_fakevr.value == 0 && vr_novrinit.value == 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, VR_GetEyeFBO(0).framebuffer);
    }

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glx, gly, glwidth, glheight);

    // draw the texture back to the framebuffer with a fragment shader
    glUseProgram(r_gamma_program);
    glUniform1f(gammaLoc, vid_gamma.value);
    glUniform1f(contrastLoc, q_min(2.0, q_max(1.0, vid_contrast.value)));
    glUniform1i(textureLoc, 0); // use texture unit 0

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glViewport(glx, gly, glwidth, glheight);

    const float smax = glwidth / (float)r_gamma_texture_width;
    const float tmax = glheight / (float)r_gamma_texture_height;

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-1, -1);
    glTexCoord2f(smax, 0);
    glVertex2f(1, -1);
    glTexCoord2f(smax, tmax);
    glVertex2f(1, 1);
    glTexCoord2f(0, tmax);
    glVertex2f(-1, 1);
    glEnd();

    glEnable(GL_CULL_FACE);

    glUseProgram(0);

    // clear cached binding
    GL_ClearBindings();
}

/*
=================
R_CullBox -- johnfitz -- replaced with new function from lordhavoc

Returns true if the box is completely outside the frustum
=================
*/
bool R_CullBox(const qvec3& emins, const qvec3& emaxs)
{
    for(int i = 0; i < 4; i++)
    {
        const mplane_t* const p = frustum + i;

        switch(p->signbits)
        {
            default: [[fallthrough]];
            case 0:
            {
                if(p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
                        p->normal[2] * emaxs[2] <
                    p->dist)
                {
                    return true;
                }

                break;
            }
            case 1:
            {
                if(p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
                        p->normal[2] * emaxs[2] <
                    p->dist)
                {
                    return true;
                }

                break;
            }
            case 2:
            {
                if(p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
                        p->normal[2] * emaxs[2] <
                    p->dist)
                {
                    return true;
                }

                break;
            }
            case 3:
            {
                if(p->normal[0] * emins[0] + p->normal[1] * emins[1] +
                        p->normal[2] * emaxs[2] <
                    p->dist)
                {
                    return true;
                }

                break;
            }
            case 4:
            {
                if(p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
                        p->normal[2] * emins[2] <
                    p->dist)
                {
                    return true;
                }

                break;
            }
            case 5:
            {
                if(p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
                        p->normal[2] * emins[2] <
                    p->dist)
                {
                    return true;
                }

                break;
            }
            case 6:
            {
                if(p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
                        p->normal[2] * emins[2] <
                    p->dist)
                {
                    return true;
                }

                break;
            }
            case 7:
            {
                if(p->normal[0] * emins[0] + p->normal[1] * emins[1] +
                        p->normal[2] * emins[2] <
                    p->dist)
                {
                    return true;
                }

                break;
            }
        }
    }

    return false;
}

/*
===============
R_CullModelForEntity -- johnfitz -- uses correct bounds based on rotation
===============
*/
bool R_CullModelForEntity(entity_t* e)
{
    qvec3 mins;
    qvec3 maxs;

    if(e->angles[0] || e->angles[2]) // pitch or roll
    {
        mins = e->origin + e->model->rmins;
        maxs = e->origin + e->model->rmaxs;
    }
    else if(e->angles[1]) // yaw
    {
        mins = e->origin + e->model->ymins;
        maxs = e->origin + e->model->ymaxs;
    }
    else // no rotation
    {
        mins = e->origin + e->model->mins;
        maxs = e->origin + e->model->maxs;
    }

    return R_CullBox(mins, maxs);
}

/*
===============
R_RotateForEntity -- johnfitz -- modified to take origin and angles instead of
pointer to entity
===============
*/
void R_RotateForEntity(
    const qvec3& origin, const qvec3& angles, unsigned char scale)
{
    glTranslatef(origin[0], origin[1], origin[2]);
    glRotatef(angles[YAW], 0, 0, 1);
    glRotatef(-angles[PITCH], 0, 1, 0);
    glRotatef(angles[ROLL], 1, 0, 0);

    if(scale != 16)
    {
        glScalef(scale / 16.0, scale / 16.0, scale / 16.0);
    }
}

/*
=============
GL_PolygonOffset -- johnfitz

negative offset moves polygon closer to camera
=============
*/
void GL_PolygonOffset(int offset)
{
    if(offset > 0)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1, offset);
    }
    else if(offset < 0)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1, offset);
    }
    else
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

//==============================================================================
//
// SETUP FRAME
//
//==============================================================================

int SignbitsForPlane(mplane_t* out)
{
    int bits = 0;
    for(int j = 0; j < 3; j++)
    {
        if(out->normal[j] < 0)
        {
            bits |= 1 << j;
        }
    }
    return bits;
}

#define DEG2RAD(a) ((a)*M_PI_DIV_180)

/*
===============
R_SetFrustum -- johnfitz -- rewritten
===============
*/
void R_SetFrustum(float fovx, float fovy)
{
    if(r_stereo.value)
    {
        fovx += 10;
    }

    if(vr_enabled.value)
    {
        fovx += 25;
    }

    frustum[0].normal = TurnVector(vpn, vright, fovx / 2 - 90);
    frustum[1].normal = TurnVector(vpn, vright, 90 - fovx / 2);
    frustum[2].normal = TurnVector(vpn, vup, 90 - fovy / 2);
    frustum[3].normal = TurnVector(vpn, vup, fovy / 2 - 90);

    for(int i = 0; i < 4; i++)
    {
        frustum[i].type = PLANE_ANYZ;
        frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
        frustum[i].signbits = SignbitsForPlane(&frustum[i]);
    }
}

#define NEARCLIP 4
float frustum_skew = 0.0;
void GL_SetFrustum(float fovx, float fovy)
{
    float xmax = NEARCLIP * tan((double)fovx * M_PI / 360.0);
    float ymax = NEARCLIP * tan((double)fovy * M_PI / 360.0);
    glFrustum(-xmax + frustum_skew, xmax + frustum_skew, -ymax, ymax, NEARCLIP,
        gl_farclip.value);
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL()
{
    int scale;

    if(VR_EnabledAndNotFake())
    {
        VR_SetMatrices();
    }
    else
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        scale = CLAMP(1, (int)r_scale.value, 4);
        glViewport(glx + r_refdef.vrect.x,
            gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height,
            r_refdef.vrect.width / scale, r_refdef.vrect.height / scale);

        GL_SetFrustum(r_fovx, r_fovy);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(-90, 1, 0, 0);
    glRotatef(90, 0, 0, 1);
    glRotatef(-r_refdef.viewangles[2], 1, 0, 0);
    glRotatef(-r_refdef.viewangles[0], 0, 1, 0);
    glRotatef(-r_refdef.viewangles[1], 0, 0, 1);
    glTranslatef(
        -r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

    if(gl_cull.value)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }

    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);
}

/*
=============
R_Clear
=============
*/
void R_Clear()
{
    unsigned int clearbits = GL_DEPTH_BUFFER_BIT;
    if(gl_stencilbits)
    {
        clearbits |= GL_STENCIL_BUFFER_BIT;
    }
    if(gl_clear.value && !skyroom_drawn)
    {
        clearbits |= GL_COLOR_BUFFER_BIT;
    }
    glClear(clearbits);
}

/*
===============
R_SetupScene
===============
*/
void R_SetupScene()
{
    R_PushDlights();
    R_AnimateLight();
    r_framecount++;
    R_SetupGL();
}

/*
===============
R_SetupView
===============
*/
void R_SetupView()
{
    Fog_SetupFrame();

    r_origin = r_refdef.vieworg;

    std::tie(vpn, vright, vup) =
        quake::util::getAngledVectors(r_refdef.viewangles);

    r_oldviewleaf = r_viewleaf;
    r_viewleaf = Mod_PointInLeaf(r_origin, cl.worldmodel);

    V_SetContentsColor(r_viewleaf->contents);
    V_CalcBlend();

    r_cache_thrash = false;

    r_fovx = r_refdef.fov_x;
    r_fovy = r_refdef.fov_y;
    if(r_waterwarp.value)
    {
        int contents = Mod_PointInLeaf(r_origin, cl.worldmodel)->contents;
        if(contents == CONTENTS_WATER || contents == CONTENTS_SLIME ||
            contents == CONTENTS_LAVA)
        {
            r_fovx = atan(tan(DEG2RAD(r_refdef.fov_x) / 2) *
                          (0.97 + sin(cl.time * 1.5) * 0.03)) *
                     2 / M_PI_DIV_180;
            r_fovy = atan(tan(DEG2RAD(r_refdef.fov_y) / 2) *
                          (1.03 - sin(cl.time * 1.5) * 0.03)) *
                     2 / M_PI_DIV_180;
        }
    }

    R_SetFrustum(r_fovx, r_fovy);
    R_MarkSurfaces();
    R_CullSurfaces();

    if(!skyroom_drawn)
    {
        R_UpdateWarpTextures();
    }

    R_Clear();

    r_drawflat_cheatsafe = r_fullbright_cheatsafe = r_lightmap_cheatsafe =
        false;
    r_drawworld_cheatsafe = true;
    if(cl.maxclients == 1)
    {
        if(!r_drawworld.value)
        {
            r_drawworld_cheatsafe = false;
        }

        if(r_drawflat.value)
        {
            r_drawflat_cheatsafe = true;
        }
        else if(r_fullbright.value || !cl.worldmodel->lightdata)
        {
            r_fullbright_cheatsafe = true;
        }
        else if(r_lightmap.value)
        {
            r_lightmap_cheatsafe = true;
        }
    }
}

//==============================================================================
//
// RENDER VIEW
//
//==============================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList(bool alphapass)
{
    if(!r_drawentities.value)
    {
        return;
    }

    for(int i = 0; i < cl_numvisedicts; i++)
    {
        currententity = cl_visedicts[i];

        if((ENTALPHA_DECODE(currententity->alpha) < 1 && !alphapass) ||
            (ENTALPHA_DECODE(currententity->alpha) == 1 && alphapass))
        {
            continue;
        }

        if(currententity == &cl.entities[cl.viewentity])
        {
            currententity->angles[0] *= 0.3;
        }

        switch(currententity->model->type)
        {
            case mod_alias: R_DrawAliasModel(currententity); break;
            case mod_brush: R_DrawBrushModel(currententity); break;
            case mod_sprite: R_DrawSpriteModel(currententity); break;
            case mod_ext_invalid:
                break;
        }
    }
}

void R_DrawWorldText()
{
    const auto drawCharacterQuad = [](const qvec3& pos, const qvec3& hInc,
                                       const qvec3& zInc, const char num)
    {
        const int row = num >> 4;
        const int col = num & 15;

        const float frow = row * 0.0625;
        const float fcol = col * 0.0625;
        const float size = 0.0625;

        const auto doVertex = [&](const qvec3& p)
        { glVertex3f(p.x, p.y, p.z); };

        glTexCoord2f(fcol, frow);
        doVertex(pos);

        glTexCoord2f(fcol + size, frow);
        doVertex(pos + hInc);

        glTexCoord2f(fcol + size, frow + size);
        doVertex(pos + hInc + zInc);

        glTexCoord2f(fcol, frow + size);
        doVertex(pos + zInc);
    };

    const auto forSplitStringView =
        [](const std::string_view str, const std::string_view delims, auto&& f)
    {
        for(auto first = str.data(), second = str.data(),
                 last = first + str.size();
            second != last && first != last; first = second + 1)
        {
            second = std::find_first_of(
                first, last, std::cbegin(delims), std::cend(delims));

            if(first != second)
            {
                f(std::string_view(first, second - first));
            }
        }
    };

    const auto drawString = [&](const qvec3& originalpos, const qvec3& angles,
                                const std::string_view str,
                                const WorldText::HAlign hAlign)
    {
        static std::vector<std::string_view> lines;

        lines.clear();
        forSplitStringView(str, "\n",
            [&](const std::string_view sv) { lines.emplace_back(sv); });

        if(lines.empty())
        {
            return;
        }

        const std::size_t longestLineSize = std::max_element(lines.begin(),
            lines.end(),
            [](const std::string_view& a, const std::string_view& b)
            { return a.size() < b.size(); })->size();

        const auto [fwd, right, up] = quake::util::getAngledVectors(angles);
        const auto hInc = right * 8.f;
        const auto zInc = qvec3{0, 0, -8.f} * up;

        const auto absmins = originalpos;
        const auto absmaxs = absmins +
                             (hInc * static_cast<float>(longestLineSize)) +
                             (zInc * static_cast<float>(lines.size()));

        const auto center = originalpos - ((absmaxs - absmins) / 2.f);

        std::size_t iLine = 0;
        for(const std::string_view& line : lines)
        {
            const std::size_t sizeDiff = longestLineSize - line.size();

            auto startPos = [&]
            {
                if(hAlign == WorldText::HAlign::Left)
                {
                    return center + (zInc * static_cast<float>(iLine));
                }

                if(hAlign == WorldText::HAlign::Center)
                {
                    return center +
                           (hInc * static_cast<float>(sizeDiff) / 2.f) +
                           (zInc * static_cast<float>(iLine));
                }

                assert(hAlign == WorldText::HAlign::Right);
                return center + (hInc * static_cast<float>(sizeDiff)) +
                       (zInc * static_cast<float>(iLine));
            }();

            for(const char c : line)
            {
                if(c != ' ')
                {
                    drawCharacterQuad(startPos, hInc, zInc, c);
                }

                startPos += hInc;
            }

            ++iLine;
        }
    };

    if(!r_drawworldtext.value)
    {
        return;
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glColor4f(1, 1, 1, 1);

    extern gltexture_t* char_texture;
    GL_Bind(char_texture);
    glBegin(GL_QUADS);

    for(const WorldText& wt : cl.worldTexts)
    {
        drawString(wt._pos, wt._angles, wt._text, wt._hAlign);
    }

    glEnd();

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel(entity_t* viewent)
{
    if(!r_drawviewmodel.value || !r_drawentities.value || chase_active.value ||
        viewent->hidden)
    {
        return;
    }

    if(cl.stats[STAT_HEALTH] <= 0)
    {
        return;
    }

    viewent->alpha = (cl.items & IT_INVISIBILITY) ? 128 : 255;

    currententity = viewent;
    if(!currententity->model)
    {
        return;
    }

    if(currententity->model->type != mod_alias)
    {
        return;
    }

    if(!vr_enabled.value)
    {
        glDepthRangef(0, 0.3f);
    }

    R_DrawAliasModel(currententity);

    if(!vr_enabled.value)
    {
        glDepthRangef(0, 1.0f);
    }
}

void R_DrawString(const qvec3& originalpos, const qvec3& angles,
    const std::string_view str, const WorldText::HAlign hAlign,
    const float scale)
{
    const auto drawCharacterQuad = [](const qvec3& pos, const qvec3& hInc,
                                       const qvec3& zInc, const char num)
    {
        const int row = num >> 4;
        const int col = num & 15;

        const float frow = row * 0.0625;
        const float fcol = col * 0.0625;
        const float size = 0.0625;

        const auto doVertex = [&](const qvec3& p)
        { glVertex3f(p.x, p.y, p.z); };

        glTexCoord2f(fcol, frow);
        doVertex(pos);

        glTexCoord2f(fcol + size, frow);
        doVertex(pos + hInc);

        glTexCoord2f(fcol + size, frow + size);
        doVertex(pos + hInc + zInc);

        glTexCoord2f(fcol, frow + size);
        doVertex(pos + zInc);
    };

    const auto forSplitStringView =
        [](const std::string_view str, const std::string_view delims, auto&& f)
    {
        for(auto first = str.data(), second = str.data(),
                 last = first + str.size();
            second != last && first != last; first = second + 1)
        {
            second = std::find_first_of(
                first, last, std::cbegin(delims), std::cend(delims));

            if(first != second)
            {
                f(std::string_view(first, second - first));
            }
        }
    };

    const auto drawString = [&]
    {
        static std::vector<std::string_view> lines;

        lines.clear();
        forSplitStringView(str, "\n",
            [&](const std::string_view sv) { lines.emplace_back(sv); });

        if(lines.empty())
        {
            return;
        }

        const std::size_t longestLineSize = std::max_element(lines.begin(),
            lines.end(),
            [](const std::string_view& a, const std::string_view& b)
            { return a.size() < b.size(); })->size();

        const auto [fwd, right, up] = quake::util::getAngledVectors(angles);
        const auto hInc = right * 8.f * scale;
        const auto zInc = up * 8.f * scale;

        const auto absmins = originalpos;
        const auto absmaxs = absmins +
                             (hInc * static_cast<float>(longestLineSize)) +
                             (zInc * static_cast<float>(lines.size()));

        const auto center = originalpos - ((absmaxs - absmins) / 2.f);

        std::size_t iLine = 0;
        for(const std::string_view& line : lines)
        {
            const std::size_t sizeDiff = longestLineSize - line.size();

            auto startPos = [&]
            {
                if(hAlign == WorldText::HAlign::Left)
                {
                    return center + (zInc * static_cast<float>(iLine));
                }

                if(hAlign == WorldText::HAlign::Center)
                {
                    return center +
                           (hInc * static_cast<float>(sizeDiff) / 2.f) +
                           (zInc * static_cast<float>(iLine));
                }

                assert(hAlign == WorldText::HAlign::Right);
                return center + (hInc * static_cast<float>(sizeDiff)) +
                       (zInc * static_cast<float>(iLine));
            }();

            for(const char c : line)
            {
                if(c != ' ')
                {
                    drawCharacterQuad(startPos, hInc, zInc, c);
                }

                startPos += hInc;
            }

            ++iLine;
        }
    };

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glColor4f(1, 1, 1, 1);

    extern gltexture_t* char_texture;
    GL_Bind(char_texture);
    glBegin(GL_QUADS);

    drawString();

    glEnd();

    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

/*
================
R_EmitWirePoint
================
*/
void R_EmitWirePoint(const qvec3& origin)
{
    constexpr int size = 4;

    glBegin(GL_LINES);
    glVertex3f(origin[0] - size, origin[1], origin[2]);
    glVertex3f(origin[0] + size, origin[1], origin[2]);
    glVertex3f(origin[0], origin[1] - size, origin[2]);
    glVertex3f(origin[0], origin[1] + size, origin[2]);
    glVertex3f(origin[0], origin[1], origin[2] - size);
    glVertex3f(origin[0], origin[1], origin[2] + size);
    glEnd();
}

/*
================
R_EmitWireBox
================
*/
void R_EmitWireBox(const qvec3& mins, const qvec3& maxs)
{
    glBegin(GL_QUAD_STRIP);
    glVertex3f(mins[0], mins[1], mins[2]);
    glVertex3f(mins[0], mins[1], maxs[2]);
    glVertex3f(maxs[0], mins[1], mins[2]);
    glVertex3f(maxs[0], mins[1], maxs[2]);
    glVertex3f(maxs[0], maxs[1], mins[2]);
    glVertex3f(maxs[0], maxs[1], maxs[2]);
    glVertex3f(mins[0], maxs[1], mins[2]);
    glVertex3f(mins[0], maxs[1], maxs[2]);
    glVertex3f(mins[0], mins[1], mins[2]);
    glVertex3f(mins[0], mins[1], maxs[2]);
    glEnd();
}

/*
================
R_ShowBoundingBoxes
================
*/
void R_ShowBoundingBoxes()
{
    if(!r_showbboxes.value || cl.maxclients > 1 || !r_drawentities.value ||
        !sv.active)
    {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_PolygonOffset(OFFSET_SHOWTRIS);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glColor3f(1, 1, 1);

    qcvm_t* oldvm = qcvm;
    PR_SwitchQCVM(nullptr);
    PR_SwitchQCVM(&sv.qcvm);

    {
        int i;
        edict_t* ed;
        for(i = 0, ed = NEXT_EDICT(qcvm->edicts); i < qcvm->num_edicts;
            i++, ed = NEXT_EDICT(ed))
        {
            if(ed == sv_player && !r_showbboxes_player.value)
            {
                continue;
            }

            if(ed->v.mins[0] == ed->v.maxs[0] &&
                ed->v.mins[1] == ed->v.maxs[1] &&
                ed->v.mins[2] == ed->v.maxs[2])
            {
                R_EmitWirePoint(ed->v.origin);
            }
            else
            {
                if(ed->v.solid == SOLID_BSP &&
                    (ed->v.angles[0] || ed->v.angles[1] || ed->v.angles[2]) &&
                    pr_checkextension.value)
                {
                    R_EmitWireBox(ed->v.absmin, ed->v.absmax);
                }
                else
                {
                    const qvec3 mins = ed->v.mins + ed->v.origin;
                    const qvec3 maxs = ed->v.maxs + ed->v.origin;
                    R_EmitWireBox(mins, maxs);
                }
            }
        }
    }

    PR_SwitchQCVM(nullptr);
    PR_SwitchQCVM(oldvm);

    glColor3f(1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    GL_PolygonOffset(OFFSET_NONE);
    glEnable(GL_DEPTH_TEST);

    Sbar_Changed();
}

/*
================
R_ShowTris
================
*/
void R_ShowTris()
{
    if(r_showtris.value < 1 || r_showtris.value > 2 || cl.maxclients > 1)
    {
        return;
    }

    if(r_showtris.value == 1)
    {
        glDisable(GL_DEPTH_TEST);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_PolygonOffset(OFFSET_SHOWTRIS);
    glDisable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);

    if(r_drawworld.value)
    {
        R_DrawWorld_ShowTris();
    }

    if(r_drawentities.value)
    {
        for(int i = 0; i < cl_numvisedicts; i++)
        {
            currententity = cl_visedicts[i];

            if(currententity == &cl.entities[cl.viewentity])
            {
                currententity->angles[0] *= 0.3;
            }

            switch(currententity->model->type)
            {
                case mod_brush: R_DrawBrushModel_ShowTris(currententity); break;
                case mod_alias: R_DrawAliasModel_ShowTris(currententity); break;
                case mod_sprite: R_DrawSpriteModel(currententity); break;
                default: break;
            }
        }

        const auto doViewmodel = [&](entity_t& ent)
        {
            currententity = &ent;
            if(r_drawviewmodel.value && !chase_active.value &&
                cl.stats[STAT_HEALTH] > 0 && !(cl.items & IT_INVISIBILITY) &&
                currententity->model && currententity->model->type == mod_alias)
            {
                glDepthRangef(0, 0.3f);
                R_DrawAliasModel_ShowTris(currententity);
                glDepthRangef(0, 1.0f);
            }
        };

        forAllViewmodels(cl, doViewmodel);
    }

    extern cvar_t r_particles;
    if(r_particles.value)
    {
        R_DrawParticles_ShowTris();
    }

    glColor3f(1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    GL_PolygonOffset(OFFSET_NONE);
    if(r_showtris.value == 1)
    {
        glEnable(GL_DEPTH_TEST);
    }

    Sbar_Changed();
}

/*
================
R_DrawShadows
================
*/
void R_DrawShadows()
{
    if(!r_shadows.value || !r_drawentities.value || r_drawflat_cheatsafe ||
        r_lightmap_cheatsafe)
    {
        return;
    }

    if(gl_stencilbits)
    {
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_EQUAL, 0, ~0);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
        glEnable(GL_STENCIL_TEST);
    }

    for(int i = 0; i < cl_numvisedicts; i++)
    {
        currententity = cl_visedicts[i];

        if(currententity->model->type != mod_alias)
        {
            continue;
        }

        const bool isViewmodel =
            anyViewmodel(cl, [&](entity_t& e) { return currententity == &e; });

        if(isViewmodel)
        {
            continue;
        }

        GL_DrawAliasShadow(currententity);
    }

    const auto drawViewentShadow = [](entity_t& ent)
    {
        if(ent.model != nullptr)
        {
            currententity = &ent;
            GL_DrawAliasShadow(&ent);
        }
    };

    {
        const auto playerShadows =
            quake::util::cvarToEnum<VrPlayerShadows>(vr_player_shadows);

        if(playerShadows == VrPlayerShadows::ViewEntities ||
            playerShadows == VrPlayerShadows::Both)
        {
            forAllViewmodels(cl, drawViewentShadow);
        }

        if(playerShadows == VrPlayerShadows::ThirdPerson ||
            playerShadows == VrPlayerShadows::Both)
        {
            drawViewentShadow(cl.entities[cl.viewentity]);
        }
    }

    if(gl_stencilbits)
    {
        glDisable(GL_STENCIL_TEST);
    }
}

/*
================
R_RenderScene
================
*/
void R_RenderScene()
{
    R_SetupScene();

    Fog_EnableGFog();

    Sky_DrawSky();

    R_DrawWorld();

    S_ExtraUpdate();

    R_DrawShadows();

    R_DrawEntitiesOnList(false);

    R_DrawWorldText();

    R_DrawWorld_Water();

    R_DrawEntitiesOnList(true);

    R_RenderDlights();

    R_DrawParticles();

    Fog_DisableGFog();

    if(vr_enabled.value)
    {
        quake::vr::showfn::show_crosshair();
    }

    R_DrawViewModel(&cl.viewent);
    R_DrawViewModel(&cl.offhand_viewent);
    R_DrawViewModel(&cl.mainhand_wpn_button);
    R_DrawViewModel(&cl.offhand_wpn_button);

    const auto drawWeaponAmmoText =
        [](const textentity_t& textEnt, const int statClipIdx,
            const int statClipSizeIdx, const int statAmmoCounterIdx)
    {
        if(textEnt.hidden)
        {
            return;
        }

        qvec3 angles = textEnt.angles;
        angles[PITCH] -= 180.f;
        angles[ROLL] *= -1.f;

        char buf[64];

        if(quake::vr::get_weapon_reloading_enabled() &&
            cl.stats[statClipSizeIdx] != 0)
        {
            sprintf(buf, "%d/%d\n%d", cl.stats[statClipIdx],
                cl.stats[statClipSizeIdx], cl.stats[statAmmoCounterIdx]);
        }
        else
        {
            sprintf(buf, "%d", cl.stats[statAmmoCounterIdx]);
        }

        R_DrawString(textEnt.origin, angles, buf, WorldText::HAlign::Center,
            0.10f * textEnt.scale);
    };

    if(vr_show_weapon_text.value)
    {
        drawWeaponAmmoText(cl.mainhand_wpn_text, STAT_WEAPONCLIP,
            STAT_WEAPONCLIPSIZE, STAT_AMMOCOUNTER);

        drawWeaponAmmoText(cl.offhand_wpn_text, STAT_WEAPONCLIP2,
            STAT_WEAPONCLIPSIZE2, STAT_AMMOCOUNTER2);
    }

    if(vr_leg_holster_model_enabled.value)
    {
        R_DrawViewModel(&cl.left_hip_holster_slot);
        R_DrawViewModel(&cl.right_hip_holster_slot);

        R_DrawViewModel(&cl.left_upper_holster_slot);
        R_DrawViewModel(&cl.right_upper_holster_slot);
    }

    R_DrawViewModel(&cl.left_hip_holster);
    R_DrawViewModel(&cl.right_hip_holster);

    R_DrawViewModel(&cl.left_upper_holster);
    R_DrawViewModel(&cl.right_upper_holster);

    const auto drawHand = [](auto& handEntities)
    {
        R_DrawViewModel(&handEntities.base);
        R_DrawViewModel(&handEntities.f_thumb);
        R_DrawViewModel(&handEntities.f_index);
        R_DrawViewModel(&handEntities.f_middle);
        R_DrawViewModel(&handEntities.f_ring);
        R_DrawViewModel(&handEntities.f_pinky);
    };

    drawHand(cl.left_hand_entities);
    drawHand(cl.right_hand_entities);

    drawHand(cl.left_hand_ghost_entities);
    drawHand(cl.right_hand_ghost_entities);

    if(vr_vrtorso_enabled.value == 1)
    {
        R_DrawViewModel(&cl.vrtorso);
    }

    R_ShowTris();
    R_ShowBoundingBoxes();

    if(vr_enabled.value)
    {
        quake::vr::showfn::draw_all_show_helpers();
    }
}

static GLuint r_scaleview_texture;
static int r_scaleview_texture_width, r_scaleview_texture_height;

/*
=============
R_ScaleView_DeleteTexture
=============
*/
void R_ScaleView_DeleteTexture()
{
    glDeleteTextures(1, &r_scaleview_texture);
    r_scaleview_texture = 0;
}

/*
================
R_ScaleView
================
*/
void R_ScaleView()
{
    float smax;
    float tmax;
    int scale;
    int srcx;
    int srcy;
    int srcw;
    int srch;

    scale = CLAMP(1, (int)r_scale.value, 4);
    srcx = glx + r_refdef.vrect.x;
    srcy = gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height;
    srcw = r_refdef.vrect.width / scale;
    srch = r_refdef.vrect.height / scale;

    if(scale == 1)
    {
        return;
    }

    GL_DisableMultitexture();

    if(!r_scaleview_texture)
    {
        glGenTextures(1, &r_scaleview_texture);
        r_scaleview_texture_width = 0;
        r_scaleview_texture_height = 0;
    }
    glBindTexture(GL_TEXTURE_2D, r_scaleview_texture);

    if(r_scaleview_texture_width < srcw || r_scaleview_texture_height < srch)
    {
        r_scaleview_texture_width = srcw;
        r_scaleview_texture_height = srch;

        if(!gl_texture_NPOT)
        {
            r_scaleview_texture_width = TexMgr_Pad(r_scaleview_texture_width);
            r_scaleview_texture_height = TexMgr_Pad(r_scaleview_texture_height);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_scaleview_texture_width,
            r_scaleview_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    glBindTexture(GL_TEXTURE_2D, r_scaleview_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, srcx, srcy, srcw, srch);

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glViewport(srcx, srcy, r_refdef.vrect.width, r_refdef.vrect.height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    smax = srcw / (float)r_scaleview_texture_width;
    tmax = srch / (float)r_scaleview_texture_height;

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-1, -1);
    glTexCoord2f(smax, 0);
    glVertex2f(1, -1);
    glTexCoord2f(smax, tmax);
    glVertex2f(1, 1);
    glTexCoord2f(0, tmax);
    glVertex2f(-1, 1);
    glEnd();

    GL_ClearBindings();
}

static bool R_SkyroomWasVisible()
{
    qmodel_t* model = cl.worldmodel;
    texture_t* t;
    size_t i;
    if(!skyroom_enabled)
    {
        return false;
    }
    for(i = 0; i < model->numtextures; i++)
    {
        t = model->textures[i];
        if(t && t->texturechains[chain_world] &&
            t->texturechains[chain_world]->flags & SURF_DRAWSKY)
        {
            return true;
        }
    }
    return false;
}

/*
================
R_RenderView
================
*/
void R_RenderView()
{
    static bool skyroom_visible;

    if(r_norefresh.value)
    {
        return;
    }

    if(!cl.worldmodel)
    {
        Sys_Error("R_RenderView: NULL worldmodel");
    }

    double time1 = 0;
    if(r_speeds.value)
    {
        glFinish();
        time1 = Sys_DoubleTime();

        rs_brushpolys = rs_aliaspolys = rs_skypolys = rs_particles =
            rs_fogpolys = rs_megatexels = rs_dynamiclightmaps = rs_aliaspasses =
                rs_skypasses = rs_brushpasses = 0;
    }
    else if(gl_finish.value)
    {
        glFinish();
    }

    skyroom_drawn = false;
    if(skyroom_enabled && skyroom_visible)
    {
        qvec3 vieworg = r_refdef.vieworg;
        qvec3 viewang = r_refdef.viewangles;

        qvec3 sky_orig(skyroom_origin[0], skyroom_origin[1], skyroom_origin[2]);
        qvec3 sky_orient(skyroom_orientation[0], skyroom_orientation[1], skyroom_orientation[2]);

        if(skyroom_origin[3] > 0.0f)
        {
            r_refdef.vieworg = sky_orig + skyroom_origin[3] * vieworg;
        }
        else
        {
            r_refdef.vieworg = sky_orig;
        }

        if(skyroom_orientation[3])
        {
            qvec3 axis[3];

            float ang = skyroom_orientation[3] * cl.time;

            if(!skyroom_orientation[0] && !skyroom_orientation[1] &&
                !skyroom_orientation[2])
            {
                skyroom_orientation[0] = 0;
                skyroom_orientation[1] = 0;
                skyroom_orientation[2] = 1;
            }

            skyroom_orientation = glm::normalize(skyroom_orientation);
            axis[0] =
                RotatePointAroundVector(sky_orient, vpn, ang);
            axis[1] =
                RotatePointAroundVector(sky_orient, vright, ang);
            axis[2] =
                RotatePointAroundVector(sky_orient, vup, ang);

            r_refdef.viewangles = VectorAngles(axis[0]);
        }

        R_SetupView();
        R_RenderScene();

        r_refdef.vieworg = vieworg;
        r_refdef.viewangles = viewang;

        skyroom_drawn = true;
    }

    R_SetupView();

    if(r_stereo.value)
    {
        qfloat eyesep = CLAMP(-8.0f, r_stereo.value, 8.0f);
        qfloat fdepth = CLAMP(32.0f, r_stereodepth.value, 1024.0f);

        std::tie(vpn, vright, vup) =
            quake::util::getAngledVectors(r_refdef.viewangles);

        glColorMask(1, 0, 0, 1);
        r_refdef.vieworg += (-0.5_qf * eyesep) * vright;
        frustum_skew = 0.5 * eyesep * NEARCLIP / fdepth;
        srand((int)(cl.time * 1000));

        R_RenderScene();

        glClear(GL_DEPTH_BUFFER_BIT);
        glColorMask(0, 1, 1, 1);
        r_refdef.vieworg += (1.0_qf * eyesep) * vright;
        frustum_skew = -frustum_skew;
        srand((int)(cl.time * 1000));

        R_RenderScene();

        glColorMask(1, 1, 1, 1);
        r_refdef.vieworg += (-0.5_qf * eyesep) * vright;
        frustum_skew = 0.0f;
    }
    else
    {
        R_RenderScene();
    }

    if(r_viewleaf->contents == CONTENTS_SOLID || r_drawflat_cheatsafe ||
        r_lightmap_cheatsafe)
    {
        skyroom_visible = false;
    }
    else
    {
        skyroom_visible = R_SkyroomWasVisible();
    }
    skyroom_drawn = false;

    R_ScaleView();

    const double time2 = Sys_DoubleTime();
    if(r_pos.value)
    {
        Con_Printf("x %i y %i z %i (pitch %i yaw %i roll %i)\n",
            (int)cl.entities[cl.viewentity].origin[0],
            (int)cl.entities[cl.viewentity].origin[1],
            (int)cl.entities[cl.viewentity].origin[2],
            (int)cl.viewangles[PITCH], (int)cl.viewangles[YAW],
            (int)cl.viewangles[ROLL]);
    }
    else if(r_speeds.value == 2)
    {
        Con_Printf(
            "%3i ms  %4i/%4i wpoly %4i/%4i epoly %3i lmap %4i/%4i sky %1.1f "
            "mtex\n",
            (int)((time2 - time1) * 1000), rs_brushpolys, rs_brushpasses,
            rs_aliaspolys, rs_aliaspasses, rs_dynamiclightmaps, rs_skypolys,
            rs_skypasses, TexMgr_FrameUsage());
    }
    else if(r_speeds.value)
    {
        Con_Printf("%3i ms  %4i wpoly %4i epoly %3i lmap\n",
            (int)((time2 - time1) * 1000), rs_brushpolys, rs_aliaspolys,
            rs_dynamiclightmaps);
    }
}
