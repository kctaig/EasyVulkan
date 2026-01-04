#version 460
#pragma shader_stage(vertex)

vec2 positions[3] = {
    {    0, -.5f },
    { -.5f,  .5f },
    {  .5f,  .5f }
};

void main(){
    // gl_VertexIndex是内置变量，表示当前顶点的索引
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}