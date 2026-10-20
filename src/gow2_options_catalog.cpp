// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// PC Options menu data: every row in on-screen order, plus the UI strings.
#include "gow2_options_catalog.h"
#include "gow2_graphics_quality.h"

namespace rex::glue::options {
namespace {

std::string_view Pick(std::string_view en, std::string_view es, Lang lang) {
  if (lang == Lang::kEs && !es.empty()) return es;
  return en;
}

Choice C(std::string_view value, std::string_view en, std::string_view es = {}) {
  return Choice{.value = value, .label_en = en, .label_es = es};
}

Choice V(std::string_view value) {
  return Choice{.value = value, .label_en = value, .label_es = value};
}

void AddDisplay(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "window_mode",
      .tab = Tab::kDisplay,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Window mode",
      .label_es = "Modo de pantalla",
      .help_en =
          "How the game fills the screen. Borderless switches to other apps instantly; exclusive "
          "fullscreen can lower latency.",
      .help_es =
          "Cómo ocupa la pantalla el juego. Sin bordes cambia de app al instante; la pantalla "
          "completa exclusiva puede bajar la latencia.",
      .choices = {
          C("windowed", "Windowed", "Ventana"),
          C("borderless", "Borderless fullscreen", "Pantalla completa sin bordes"),
          C("exclusive", "Exclusive fullscreen", "Pantalla completa exclusiva"),
      }});
  out.push_back(Setting{
      .id = "monitor",
      .tab = Tab::kDisplay,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .dynamic_choices = true,
      .label_en = "Display",
      .label_es = "Monitor",
      .help_en = "The monitor the game shows on.",
      .help_es = "El monitor donde se ve el juego."});
  out.push_back(Setting{
      .id = "window_size",
      .tab = Tab::kDisplay,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Window size",
      .label_es = "Tamaño de la ventana",
      .help_en = "Size of the window in windowed mode.",
      .help_es = "Tamaño de la ventana en modo ventana.",
      .choices = {
          C("auto", "Game resolution", "Resolución del juego"),
          V("1280x720"),
          V("1600x900"),
          V("1920x1080"),
          V("2560x1440"),
          V("3840x2160"),
      },
      .visible_if_id = "window_mode",
      .visible_if_value = "windowed"});
  out.push_back(Setting{
      .id = "gow2_gpu",
      .tab = Tab::kDisplay,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kWindows,
      .dynamic_choices = true,
      .label_en = "Graphics card",
      .label_es = "Tarjeta gráfica",
      .help_en =
          "The GPU the game renders on. Automatic picks the one Windows sets as high performance.",
      .help_es =
          "La GPU que dibuja el juego. Automática usa la que Windows marca como de alto rendimiento."});
  out.push_back(Setting{
      .id = "vsync",
      .tab = Tab::kDisplay,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "VSync",
      .label_es = "Sincronización vertical",
      .help_en = "Syncs frames to the display to stop tearing. Off lowers latency.",
      .help_es =
          "Sincroniza los cuadros con la pantalla para evitar cortes. Apagada baja la latencia."});
  out.push_back(Setting{
      .id = "gow2_unlock_fps",
      .tab = Tab::kDisplay,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Unlock frame rate",
      .label_es = "Desbloquear FPS",
      .help_en =
          "Lifts the game's 30 fps cap (community patch). Turn VSync off to go above your display's "
          "refresh rate.",
      .help_es =
          "Quita el tope de 30 FPS del juego (parche de la comunidad). Apaga la sincronización "
          "vertical para pasar la frecuencia del monitor."});
  out.push_back(Setting{
      .id = "gow2_fps_cap",
      .tab = Tab::kDisplay,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Frame rate limit",
      .label_es = "Límite de FPS",
      .help_en = "Requires Unlock frame rate. Limits game frames; VSync and performance can lower FPS.",
      .help_es = "Requiere Desbloquear FPS. Limita los cuadros del juego; VSync y el rendimiento pueden bajar los FPS.",
      .choices = {
          C("0", "Off", "Sin límite"),
          V("30"),
          V("60"),
          V("75"),
          V("90"),
          V("120"),
          V("144"),
          V("165"),
          V("240"),
      }});
  out.push_back(Setting{
      .id = "present_android_frame_rate",
      .tab = Tab::kDisplay,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kAndroid,
      .label_en = "Display refresh hint",
      .label_es = "Frecuencia de pantalla",
      .help_en = "Refresh rate the game asks Android for. 30 saves battery and heat.",
      .help_es = "Frecuencia que el juego le pide a Android. 30 ahorra batería y calor.",
      .choices = {
          C("0", "Phone default", "La del teléfono"),
          V("30"),
          V("60"),
          V("90"),
          V("120"),
      }});
}

void AddGraphicsUpscaling(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "resolution_scale",
      .tab = Tab::kGraphics,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Internal resolution",
      .label_es = "Resolución interna",
      .help_en = "Resolution the game renders at. Higher is sharper and costs more GPU time. The default is automatic: 1440p on a dedicated GPU, 720p on an integrated one.",
      .help_es =
          "Resolución a la que se dibuja el juego. Más alta se ve más nítida y cuesta más GPU. El valor por defecto es automático: 1440p con GPU dedicada, 720p con integrada.",
      .choices = {
          C("1", "720p (original)", "720p (original)"),
          C("2", "1440p"),
          C("3", "2160p (4K)"),
      }});
  out.push_back(Setting{
      .id = "upscaler",
      .tab = Tab::kGraphics,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .need = Need::kNoDlss,
      .label_en = "Upscaler",
      .label_es = "Reescalador",
      .help_en =
          "Temporal upscaling from the internal resolution to your screen. Lower the internal "
          "resolution to give it room.",
      .help_es =
          "Reescalado temporal de la resolución interna a tu pantalla. Baja la resolución interna "
          "para que tenga margen.",
      .choices = {
          C("off", "Off", "Apagado"),
          C("fsr3", "AMD FSR 3.1"),
          C("fsr4", "AMD FSR 4 (Radeon RX 7000/9000)"),
      }});
  out.push_back(Setting{
      .id = "upscaler_dlss",
      .tab = Tab::kGraphics,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .need = Need::kDlss,
      .label_en = "Upscaler",
      .label_es = "Reescalador",
      .help_en =
          "Temporal upscaling from the internal resolution to your screen. Lower the internal "
          "resolution to give it room.",
      .help_es =
          "Reescalado temporal de la resolución interna a tu pantalla. Baja la resolución interna "
          "para que tenga margen.",
      .choices = {
          C("off", "Off", "Apagado"),
          C("fsr3", "AMD FSR 3.1"),
          C("fsr4", "AMD FSR 4 (Radeon RX 7000/9000)"),
          C("dlss", "NVIDIA DLSS"),
      }});
  out.push_back(Setting{
      .id = "upscale_ratio",
      .tab = Tab::kGraphics,
      .kind = Kind::kInfo,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Upscaling",
      .label_es = "Reescalado",
      .help_en = "The upscaler's quality follows the ratio between the internal resolution and the screen.",
      .help_es =
          "La calidad del reescalador depende de la relación entre la resolución interna y la pantalla.",
      .visible_if_id = "upscaler",
      .visible_if_value = "!off"});
  out.push_back(Setting{
      .id = "present_effect",
      .tab = Tab::kGraphics,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kWindows,
      .need = Need::kFidelityFx,
      .label_en = "Final image filter",
      .label_es = "Filtro de imagen final",
      .help_en = "Filter used to fit the finished image to the screen.",
      .help_es = "Filtro con el que la imagen terminada se ajusta a la pantalla.",
      .choices = {
          C("bilinear", "Standard (bilinear)", "Estándar (bilineal)"),
          C("fsr", "AMD FSR 1"),
          C("cas", "AMD CAS sharpening", "Nitidez AMD CAS"),
      }});
  out.push_back(Setting{
      .id = "present_effect_android",
      .tab = Tab::kGraphics,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kAndroid,
      .label_en = "Final image filter",
      .label_es = "Filtro de imagen final",
      .help_en = "Filter used to fit the finished image to the screen.",
      .help_es = "Filtro con el que la imagen terminada se ajusta a la pantalla.",
      .choices = {
          C("bilinear", "Standard (bilinear)", "Estándar (bilineal)"),
          C("fsr", "AMD FSR 1"),
          C("cas", "AMD CAS sharpening", "Nitidez AMD CAS"),
          C("sgsr", "Snapdragon GSR"),
      }});
  out.push_back(Setting{
      .id = "present_fsr_sharpness_reduction",
      .tab = Tab::kGraphics,
      .kind = Kind::kNumber,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "FSR 1 softening",
      .label_es = "Suavizado de FSR 1",
      .help_en = "0 is the sharpest.",
      .help_es = "0 es lo más nítido.",
      .min = 0,
      .max = 2,
      .step = 0.1,
      .decimals = 1,
      .visible_if_id = "present_effect",
      .visible_if_value = "fsr"});
  out.push_back(Setting{
      .id = "present_cas_additional_sharpness",
      .tab = Tab::kGraphics,
      .kind = Kind::kNumber,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "CAS sharpness",
      .label_es = "Nitidez de CAS",
      .help_en = "Extra sharpening on top of CAS.",
      .help_es = "Nitidez extra sobre CAS.",
      .min = 0,
      .max = 1,
      .step = 0.05,
      .decimals = 2,
      .visible_if_id = "present_effect",
      .visible_if_value = "cas"});
  out.push_back(Setting{
      .id = "present_dither",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Dithering",
      .label_es = "Tramado (dithering)",
      .help_en = "Hides colour banding in smoke, sky and dark corridors.",
      .help_es = "Disimula las bandas de color en humo, cielo y pasillos oscuros."});
  out.push_back(Setting{
      .id = "anisotropic_override",
      .tab = Tab::kGraphics,
      .kind = Kind::kChoice,
      .apply = Apply::kNextLevel,
      .platforms = kAllPlatforms,
      .label_en = "Anisotropic filtering",
      .label_es = "Filtrado anisotrópico",
      .help_en = "Keeps floors and walls sharp at steep angles. The default is automatic: 8x on a dedicated GPU, 2x on an integrated one.",
      .help_es = "Mantiene nítidos suelos y paredes vistos de lado. El valor por defecto es automático: 8x con GPU dedicada, 2x con integrada.",
      .choices = {
          C("-1", "Game default", "El del juego"),
          C("0", "Off", "Apagado"),
          C("1", "1x"),
          C("2", "2x"),
          C("3", "4x"),
          C("4", "8x"),
          C("5", "16x"),
      }});
  out.push_back(Setting{
      .id = "texture_memory",
      .tab = Tab::kGraphics,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Texture cache",
      .label_es = "Caché de texturas",
      .help_en =
          "Memory the runtime keeps for textures. More means less stutter when it fits in your GPU.",
      .help_es =
          "Memoria que el runtime guarda para texturas. Más significa menos tirones si cabe en tu GPU.",
      .choices = {
          C("384/768", "Low (384 MB)", "Baja (384 MB)"),
          C("1024/2048", "Medium (1 GB)", "Media (1 GB)"),
          C("2048/4096", "High (2 GB)", "Alta (2 GB)"),
          C("4096/8192", "Very high (4 GB)", "Muy alta (4 GB)"),
      }});
}

struct QualityLabel {
  std::string_view key, en, es, help_en, help_es;
};

constexpr QualityLabel kQualityLabels[] = {
    {"LightEnvironmentShadows", "Object shadows", "Sombras de objetos", "Shadows from characters and movable objects.", "Sombras de personajes y objetos móviles."},
    {"DynamicShadows", "Dynamic shadows", "Sombras dinámicas", "Shadows that move with lights and objects.", "Sombras que se mueven con las luces y los objetos."},
    {"DynamicLights", "Dynamic lights", "Luces dinámicas", "Lighting from moving and changing light sources.", "Iluminación de fuentes de luz móviles y cambiantes."},
    {"CompositeDynamicLights", "Composite dynamic lights", "Luces dinámicas combinadas", "Combines dynamic lights for cheaper object lighting.", "Combina luces dinámicas para iluminar objetos con menor coste."},
    {"Bloom", "Bloom", "Resplandor", "Glow around bright lights and highlights.", "Resplandor alrededor de luces y reflejos brillantes."},
    {"UseHighQualityBloom", "High quality bloom", "Resplandor de alta calidad", "Uses the higher quality bloom filter.", "Usa el filtro de resplandor de mayor calidad."},
    {"MotionBlur", "Motion blur", "Desenfoque de movimiento", "Blurs movement to soften rapid motion.", "Desenfoca los movimientos rápidos."},
    {"DepthOfField", "Depth of field", "Profundidad de campo", "Blurs objects outside the camera's focus.", "Desenfoca los objetos fuera del foco de la cámara."},
    {"AmbientOcclusion", "Ambient occlusion", "Oclusión ambiental", "Adds contact shading in corners and creases.", "Añade sombras de contacto en rincones y pliegues."},
    {"LensFlares", "Lens flares", "Destellos de lente", "Camera glare from bright light sources.", "Destellos de cámara de las fuentes de luz brillantes."},
    {"Distortion", "Distortion", "Distorsión", "Heat haze and other screen distortion effects.", "Ondas de calor y otros efectos de distorsión de pantalla."},
    {"StaticDecals", "Static decals", "Calcomanías estáticas", "Surface details such as stains and marks on scenery.", "Detalles como manchas y marcas sobre el escenario."},
    {"DynamicDecals", "Dynamic decals", "Calcomanías dinámicas", "Runtime surface marks such as bullet impacts.", "Marcas creadas durante el juego, como impactos de bala."},
    {"FogVolumes", "Fog volumes", "Volúmenes de niebla", "Local fog volumes used by the game's environments.", "Volúmenes locales de niebla de los escenarios."},
    {"SpeedTreeLeaves", "Tree leaves", "Hojas de árboles", "Draws leaves on SpeedTree vegetation.", "Dibuja las hojas de la vegetación SpeedTree."},
    {"SpeedTreeFronds", "Tree fronds", "Frondas de árboles", "Draws fronds on SpeedTree vegetation.", "Dibuja las frondas de la vegetación SpeedTree."},
    {"ShadowFilterQualityBias", "Shadow filter bias", "Sesgo del filtro de sombras", "Biases the game's shadow filter quality; lower is cheaper.", "Ajusta la calidad del filtro de sombras; menos cuesta menos."},
    {"DetailMode", "World detail", "Detalle del mundo", "Controls detail objects; higher draws more detail.", "Controla los objetos de detalle; más alto dibuja más detalle."},
    {"SkeletalMeshLODBias", "Character LOD bias", "Sesgo de detalle de personajes", "Higher values use simpler character meshes sooner.", "Valores mayores usan antes modelos de personajes más simples."},
    {"ParticleLODBias", "Particle LOD bias", "Sesgo de detalle de partículas", "Higher values use simpler particle effects sooner.", "Valores mayores usan antes efectos de partículas más simples."},
    {"FoliageDrawRadiusMultiplier", "Foliage draw radius", "Radio de dibujado de vegetación", "Scales how far foliage is drawn; higher costs more.", "Escala la distancia de dibujado de vegetación; más alto cuesta más."},
    {"MaxShadowResolution", "Maximum shadow resolution", "Resolución máxima de sombras", "Caps shadow map resolution; higher gives sharper shadows.", "Limita la resolución de sombras; más alto da sombras más nítidas."},
    {"ShadowTexelsPerPixel", "Shadow texels per pixel", "Texeles de sombra por píxel", "Shadow detail relative to screen pixels; higher costs more.", "Detalle de sombras relativo a los píxeles; más alto cuesta más."},
    {"DecalCullDistanceScale", "Decal draw distance", "Distancia de dibujado de calcomanías", "Scales how far surface marks remain visible.", "Escala la distancia a la que siguen visibles las marcas."},
};

std::vector<Choice> QualityChoices(const graphics_quality::Setting& setting) {
  std::vector<Choice> choices{C("auto", "Platform default", "Predeterminado de plataforma")};
  for (const auto value : setting.choices) {
    if (value == "auto") continue;
    if (value == "on") choices.push_back(C(value, "On", "Encendido"));
    else if (value == "off") choices.push_back(C(value, "Off", "Apagado"));
    else choices.push_back(V(value));
  }
  return choices;
}

void AddGraphicsQuality(std::vector<Setting>& out) {
  out.push_back(Setting{.id = "game_quality_group", .tab = Tab::kGraphics,
      .kind = Kind::kInfo, .label_en = "Game quality", .label_es = "Calidad del juego"});
  out.push_back(Setting{.id = "gow2_gfx_preset", .tab = Tab::kGraphics,
      .kind = Kind::kChoice, .apply = Apply::kRestart,
      .label_en = "Quality preset", .label_es = "Preajuste de calidad",
      .help_en = "Sets game quality together; individual changes show Custom. Restart required. Raw engine settings take priority.",
      .help_es = "Ajusta la calidad en conjunto; los cambios individuales muestran Personalizado. Requiere reiniciar. Los ajustes del motor tienen prioridad.",
      .choices = {C("low", "Low", "Bajo"), C("medium", "Medium", "Medio"),
                  C("high", "High (retail)", "Alto (original)"), C("custom", "Custom", "Personalizado")}});
  for (const auto& setting : graphics_quality::Settings()) {
    for (const auto& label : kQualityLabels) {
      if (label.key != setting.key) continue;
      out.push_back(Setting{.id = setting.id, .tab = Tab::kGraphics,
          .kind = Kind::kChoice, .apply = Apply::kRestart,
          .label_en = label.en, .label_es = label.es,
          .help_en = label.help_en, .help_es = label.help_es,
          .choices = QualityChoices(setting)});
      break;
    }
  }
}

void AddGraphicsPatches(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "gow2_black_shading_fix",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Character lighting fix",
      .label_es = "Arreglo de luz en personajes",
      .help_en = "Community patch: correct lighting on characters.",
      .help_es = "Parche de la comunidad: luz correcta en los personajes."});
  out.push_back(Setting{
      .id = "gow2_texture_streaming_fix",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Bigger texture pool",
      .label_es = "Más memoria de texturas del juego",
      .help_en = "Community patch: less texture pop-in.",
      .help_es = "Parche de la comunidad: menos texturas que aparecen tarde."});
  out.push_back(Setting{
      .id = "gow2_aniso16",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "16x filtering patch",
      .label_es = "Parche de filtrado 16x",
      .help_en = "Community patch: 16x anisotropic filtering in the game's own sampler setup.",
      .help_es =
          "Parche de la comunidad: filtrado anisotrópico 16x en la configuración del propio juego."});
  out.push_back(Setting{
      .id = "gow2_ao_upscaling_fix",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Ambient occlusion fix",
      .label_es = "Arreglo de oclusión ambiental",
      .help_en = "Community patch: ambient occlusion without checkerboard artifacts above 720p.",
      .help_es =
          "Parche de la comunidad: oclusión ambiental sin cuadriculado por encima de 720p."});
  out.push_back(Setting{
      .id = "gow2_dof_upscaling_fix",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Depth of field fix",
      .label_es = "Arreglo de profundidad de campo",
      .help_en = "Community patch: depth of field without blur artifacts above 720p.",
      .help_es =
          "Parche de la comunidad: profundidad de campo sin artefactos por encima de 720p."});
  out.push_back(Setting{
      .id = "texture_replace",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kWindows,
      .label_en = "Texture packs",
      .label_es = "Packs de texturas",
      .help_en = "Loads your own textures from textures\\4D53082D\\replace next to the game.",
      .help_es = "Carga tus propias texturas desde textures\\4D53082D\\replace junto al juego."});
  out.push_back(Setting{
      .id = "texture_dump",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kWindows,
      .advanced = true,
      .label_en = "Dump textures",
      .label_es = "Volcar texturas",
      .help_en = "Saves every new texture to textures\\4D53082D\\dump to make packs.",
      .help_es = "Guarda cada textura nueva en textures\\4D53082D\\dump para hacer packs."});
  out.push_back(Setting{
      .id = "gow2_android_sustained_performance",
      .tab = Tab::kGraphics,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kAndroid,
      .label_en = "Sustained performance",
      .label_es = "Rendimiento sostenido",
      .help_en = "Asks Android for steady clocks: fewer drops when the phone is hot, a lower peak.",
      .help_es =
          "Le pide a Android relojes estables: menos caídas con el teléfono caliente, un pico más bajo."});
  out.push_back(Setting{
      .id = "gow2_system_settings",
      .tab = Tab::kGraphics,
      .kind = Kind::kText,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .advanced = true,
      .label_en = "Engine settings",
      .label_es = "Ajustes del motor",
      .help_en = "Overrides Game quality rows with raw [SystemSettings], e.g. LightEnvironmentShadows=0;DynamicShadows=0.",
      .help_es =
          "Sobrescribe Calidad del juego con [SystemSettings], p. ej. LightEnvironmentShadows=0;DynamicShadows=0.",
      .max_length = 200});
}

void AddControls(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "mnk_mode",
      .tab = Tab::kControls,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Keyboard and mouse",
      .label_es = "Teclado y mouse",
      .help_en = "Plays with keyboard and mouse; a controller still works at the same time.",
      .help_es = "Juega con teclado y mouse; un mando sigue funcionando a la vez."});
  out.push_back(Setting{
      .id = "gow2_mouse_aim",
      .tab = Tab::kControls,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Native mouse aim",
      .label_es = "Puntería nativa con mouse",
      .help_en =
          "Mouse look through the game's own PC mouse axes, with no stick dead zone. Off: the mouse "
          "acts as a right stick.",
      .help_es =
          "La cámara usa los ejes de mouse de PC del propio juego, sin zona muerta. Apagado: el mouse "
          "actúa como stick derecho."});
  out.push_back(Setting{
      .id = "gow2_mouse_scale",
      .tab = Tab::kControls,
      .kind = Kind::kNumber,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .need = Need::kNativeMouse,
      .label_en = "Mouse sensitivity",
      .label_es = "Sensibilidad del mouse",
      .help_en = "How far the camera turns per mouse movement.",
      .help_es = "Cuánto gira la cámara por movimiento del mouse.",
      .min = 0.05,
      .max = 10,
      .step = 0.05,
      .decimals = 2,
      .unit = "x"});
  out.push_back(Setting{
      .id = "mnk_sensitivity",
      .tab = Tab::kControls,
      .kind = Kind::kNumber,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .need = Need::kNoNativeMouse,
      .label_en = "Mouse sensitivity",
      .label_es = "Sensibilidad del mouse",
      .help_en = "How far the camera turns per mouse movement.",
      .help_es = "Cuánto gira la cámara por movimiento del mouse.",
      .min = 0.05,
      .max = 10,
      .step = 0.05,
      .decimals = 2,
      .unit = "x"});
  out.push_back(Setting{
      .id = "gow2_mouse_invert_y",
      .tab = Tab::kControls,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .need = Need::kNativeMouse,
      .label_en = "Invert mouse Y",
      .label_es = "Invertir eje Y del mouse",
      .help_en = "Moving the mouse forward looks down.",
      .help_es = "Mover el mouse hacia adelante mira hacia abajo."});
  out.push_back(Setting{
      .id = "gow2_mouse_aim_assist",
      .tab = Tab::kControls,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .need = Need::kNativeMouse,
      .label_en = "Aim assist with the mouse",
      .label_es = "Asistencia de puntería con mouse",
      .help_en = "Keeps the game's aim friction and magnetism for the mouse too.",
      .help_es = "Mantiene la fricción y el magnetismo de puntería del juego también con mouse."});
  out.push_back(Setting{
      .id = "gow2_prompts",
      .tab = Tab::kControls,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Button prompts",
      .label_es = "Indicaciones de botones",
      .help_en = "Which buttons the menus show.",
      .help_es = "Qué botones muestran los menús.",
      .choices = {
          C("auto", "Automatic", "Automático"),
          C("keyboard", "Keyboard keys", "Teclas"),
          C("controller", "Controller buttons", "Botones del mando"),
      }});
  out.push_back(Setting{
      .id = "gow2_keycaps",
      .tab = Tab::kControls,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kWindows,
      .label_en = "Keycap pictures",
      .label_es = "Imágenes de teclas",
      .help_en = "Shows the keys as keycaps (needs the pack from scripts/build-keycaps.ps1) instead of text.",
      .help_es = "Muestra las teclas como teclas dibujadas (necesita el paquete de scripts/build-keycaps.ps1) en vez de texto."});
  out.push_back(Setting{
      .id = "pad_right_stick_deadzone",
      .tab = Tab::kControls,
      .kind = Kind::kNumber,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Controller look dead zone",
      .label_es = "Zona muerta del stick derecho",
      .help_en = "Ignores small right-stick movements. Raise it if the camera drifts.",
      .help_es = "Ignora movimientos pequeños del stick derecho. Súbela si la cámara se mueve sola.",
      .min = 0,
      .max = 0.9,
      .step = 0.05,
      .decimals = 2});
  out.push_back(Setting{
      .id = "keybindings",
      .tab = Tab::kControls,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Key bindings",
      .label_es = "Asignación de teclas",
      .help_en = "Change the key for each action.",
      .help_es = "Cambia la tecla de cada acción."});
  out.push_back(Setting{
      .id = "controller_layout",
      .tab = Tab::kControls,
      .kind = Kind::kInfo,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Controller layout",
      .label_es = "Disposición del mando",
      .help_en = "Controller button layouts are in the game's own Options > Controller.",
      .help_es = "Las disposiciones de botones del mando están en Opciones > Mando del propio juego."});
}

void AddKeybind(std::vector<Setting>& out, std::string_view id, std::string_view en,
                std::string_view es) {
  out.push_back(Setting{
      .id = id,
      .tab = Tab::kControls,
      .page = Page::kKeybinds,
      .kind = Kind::kKeybind,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = en,
      .label_es = es,
      .help_en =
          "Accept, then press the new key. It works at once, in a level too. Default restores the "
          "original keys. Menu prompts update after a restart.",
      .help_es =
          "Acepta y pulsa la nueva tecla. Funciona al instante, también en un nivel. Predeterminado "
          "restaura las teclas originales. Las indicaciones de los menús cambian al reiniciar."});
}

void AddKeybinds(std::vector<Setting>& out) {
  AddKeybind(out, "keybind_lstick_up", "Move forward", "Avanzar");
  AddKeybind(out, "keybind_lstick_down", "Move back", "Retroceder");
  AddKeybind(out, "keybind_lstick_left", "Move left", "Moverse a la izquierda");
  AddKeybind(out, "keybind_lstick_right", "Move right", "Moverse a la derecha");
  AddKeybind(out, "keybind_rstick_up", "Look up", "Mirar arriba");
  AddKeybind(out, "keybind_rstick_down", "Look down", "Mirar abajo");
  AddKeybind(out, "keybind_rstick_left", "Look left", "Mirar a la izquierda");
  AddKeybind(out, "keybind_rstick_right", "Look right", "Mirar a la derecha");
  AddKeybind(out, "keybind_right_trigger", "Fire", "Disparar");
  AddKeybind(out, "keybind_left_trigger", "Aim", "Apuntar");
  AddKeybind(out, "keybind_a", "Cover, run, roll", "Cobertura, correr, rodar");
  AddKeybind(out, "keybind_b", "Melee, chainsaw", "Golpe, motosierra");
  AddKeybind(out, "keybind_x", "Use, pick up, revive", "Usar, recoger, revivir");
  AddKeybind(out, "keybind_y", "Point of interest", "Punto de interés");
  AddKeybind(out, "keybind_right_shoulder", "Reload", "Recargar");
  AddKeybind(out, "keybind_left_shoulder", "Tactical display", "Pantalla táctica");
  AddKeybind(out, "keybind_rstick_press", "Zoom", "Zoom");
  AddKeybind(out, "keybind_lstick_press", "Left stick click", "Clic del stick izquierdo");
  AddKeybind(out, "keybind_dpad_up", "Weapon 1", "Arma 1");
  AddKeybind(out, "keybind_dpad_right", "Weapon 2", "Arma 2");
  AddKeybind(out, "keybind_dpad_down", "Weapon 3", "Arma 3");
  AddKeybind(out, "keybind_dpad_left", "Weapon 4", "Arma 4");
  AddKeybind(out, "keybind_back", "Back", "Atrás");
  AddKeybind(out, "keybind_start", "Pause", "Pausa");
}

void AddTouch(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "gow2_touch",
      .tab = Tab::kTouch,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kAndroid,
      .label_en = "Touch controls",
      .label_es = "Controles táctiles",
      .help_en = "On-screen controls. Automatic shows them on a touchscreen.",
      .help_es = "Controles en pantalla. Automático los muestra si hay pantalla táctil.",
      .choices = {
          C("auto", "Automatic", "Automático"),
          C("on", "On", "Sí"),
          C("off", "Off", "No"),
      }});
  out.push_back(Setting{
      .id = "gow2_touch_opacity",
      .tab = Tab::kTouch,
      .kind = Kind::kNumber,
      .apply = Apply::kNow,
      .platforms = kAndroid,
      .need = Need::kTouch,
      .label_en = "Opacity",
      .label_es = "Opacidad",
      .help_en = "How visible the controls are.",
      .help_es = "Qué tan visibles son los controles.",
      .min = 0,
      .max = 1,
      .step = 0.05,
      .decimals = 0,
      .display_scale = 100,
      .unit = "%"});
  out.push_back(Setting{
      .id = "gow2_touch_scale",
      .tab = Tab::kTouch,
      .kind = Kind::kNumber,
      .apply = Apply::kNow,
      .platforms = kAndroid,
      .need = Need::kTouch,
      .label_en = "Size",
      .label_es = "Tamaño",
      .help_en = "Size of the on-screen controls.",
      .help_es = "Tamaño de los controles en pantalla.",
      .min = 0.5,
      .max = 2,
      .step = 0.05,
      .decimals = 2,
      .unit = "x"});
  out.push_back(Setting{
      .id = "gow2_touch_look_gain",
      .tab = Tab::kTouch,
      .kind = Kind::kNumber,
      .apply = Apply::kNow,
      .platforms = kAndroid,
      .need = Need::kTouch,
      .label_en = "Look sensitivity",
      .label_es = "Sensibilidad de la cámara",
      .help_en = "How fast dragging turns the camera.",
      .help_es = "Qué tan rápido gira la cámara al arrastrar.",
      .min = 0.1,
      .max = 5,
      .step = 0.1,
      .decimals = 1,
      .unit = "x"});
  out.push_back(Setting{
      .id = "gow2_touch_aim_toggle",
      .tab = Tab::kTouch,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAndroid,
      .need = Need::kTouch,
      .label_en = "Aim: tap to toggle",
      .label_es = "Apuntar: tocar para alternar",
      .help_en = "On: tap the aim button once to aim and again to stop. Off: hold it.",
      .help_es = "Sí: toca apuntar una vez para apuntar y otra para soltar. No: mantenlo pulsado."});
  out.push_back(Setting{
      .id = "gow2_touch_hide_seconds",
      .tab = Tab::kTouch,
      .kind = Kind::kNumber,
      .apply = Apply::kNow,
      .platforms = kAndroid,
      .need = Need::kTouch,
      .label_en = "Hide after keyboard or mouse",
      .label_es = "Ocultar al usar teclado o mouse",
      .help_en = "Seconds the controls stay hidden after a key or mouse input.",
      .help_es = "Segundos que los controles quedan ocultos tras usar teclado o mouse.",
      .min = 0,
      .max = 600,
      .step = 5,
      .decimals = 0,
      .unit = " s"});
}

void AddMultiplayer(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "user_name",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kText,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Your name",
      .label_es = "Tu nombre",
      .help_en = "The name other players see (up to 15 characters).",
      .help_es = "El nombre que ven los demás (hasta 15 caracteres).",
      .max_length = 15});
  out.push_back(Setting{
      .id = "net_link_active",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "System Link (LAN and internet)",
      .label_es = "System Link (LAN e internet)",
      .help_en = "Lets you host or join co-op over the network. Every player turns it on.",
      .help_es = "Permite alojar o unirse a co-op por red. Cada jugador lo activa."});
  out.push_back(Setting{
      .id = "gow2_coop_players",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Co-op players",
      .label_es = "Jugadores en co-op",
      .help_en = "Campaign co-op player cap. Every player must choose the same number.",
      .help_es = "Tope de jugadores del co-op de campaña. Todos deben elegir el mismo número.",
      .choices = {
          C("2", "2 (original)"),
          C("3", "3"),
          C("4", "4"),
      }});
  out.push_back(Setting{
      .id = "coop_check",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kInfo,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Co-op check code",
      .label_es = "Código de comprobación",
      .help_en =
          "With 3 or 4 players everyone must see the same code: same build and co-op settings.",
      .help_es =
          "Con 3 o 4 jugadores todos deben ver el mismo código: misma versión y ajustes de co-op."});
  out.push_back(Setting{
      .id = "net_peers",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kText,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .label_en = "Join a host (name or IP)",
      .label_es = "Unirse a un anfitrión (nombre o IP)",
      .help_en =
          "The address of the friend who hosts over the internet, e.g. gow2.example.com or "
          "203.0.113.7. Several: separate with commas. Empty on a LAN.",
      .help_es =
          "La dirección del amigo que aloja por internet, p. ej. gow2.example.com o 203.0.113.7. "
          "Varias: sepáralas con comas. Vacío en una LAN.",
      .max_length = 255});
  out.push_back(Setting{
      .id = "host_online",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Host over the internet",
      .label_es = "Alojar por internet",
      .help_en = "Open the System Link ports on your routers (UPnP) and choose who may join.",
      .help_es = "Abre los puertos de System Link en tus routers (UPnP) y elige quién puede unirse."});
  out.push_back(Setting{
      .id = "net_local_ip",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kText,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .advanced = true,
      .label_en = "This device's LAN address",
      .label_es = "Dirección LAN de este equipo",
      .help_en = "Only when the automatic choice is wrong, e.g. 192.168.1.10/24.",
      .help_es = "Solo si la elección automática falla, p. ej. 192.168.1.10/24.",
      .max_length = 18});
  out.push_back(Setting{
      .id = "net_port_offset",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kNumber,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .advanced = true,
      .label_en = "Port offset",
      .label_es = "Desplazamiento de puertos",
      .help_en = "Every player needs the same value. The game's UDP 1000 becomes 1000 + this.",
      .help_es = "Todos los jugadores necesitan el mismo valor. El UDP 1000 del juego pasa a 1000 + este.",
      .min = 1024,
      .max = 64512,
      .step = 1000,
      .decimals = 0});
  out.push_back(Setting{
      .id = "net_beacon_gate",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kToggle,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .advanced = true,
      .label_en = "Co-op check",
      .label_es = "Comprobación de co-op",
      .help_en = "Hides campaigns whose co-op check code differs. Turn off only to diagnose.",
      .help_es = "Oculta campañas con otro código de comprobación. Apágalo solo para diagnosticar."});
  out.push_back(Setting{
      .id = "system_link_trace",
      .tab = Tab::kMultiplayer,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .advanced = true,
      .label_en = "Network log",
      .label_es = "Registro de red",
      .help_en = "Logs every network call ([netlink] lines).",
      .help_es = "Registra cada llamada de red (líneas [netlink])."});
}

void AddHost(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "host_status",
      .tab = Tab::kMultiplayer,
      .page = Page::kHost,
      .kind = Kind::kInfo,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Status",
      .label_es = "Estado",
      .help_en = "Whether the ports are open on your routers right now.",
      .help_es = "Si los puertos están abiertos ahora en tus routers."});
  out.push_back(Setting{
      .id = "gow2_host_lease_hours",
      .tab = Tab::kMultiplayer,
      .page = Page::kHost,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Keep open for",
      .label_es = "Mantener abierto",
      .help_en = "The routers close the ports by themselves after this time.",
      .help_es = "Los routers cierran los puertos solos pasado este tiempo.",
      .choices = {
          C("2", "2 h"),
          C("6", "6 h"),
          C("12", "12 h"),
          C("24", "24 h"),
      }});
  out.push_back(Setting{
      .id = "host_open",
      .tab = Tab::kMultiplayer,
      .page = Page::kHost,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Open ports now",
      .label_es = "Abrir puertos ahora",
      .help_en =
          "Asks every UPnP router between this PC and the internet to forward UDP 14001 and 41000 here.",
      .help_es =
          "Pide a cada router UPnP entre este PC e internet que reenvíe aquí UDP 14001 y 41000."});
  out.push_back(Setting{
      .id = "host_close",
      .tab = Tab::kMultiplayer,
      .page = Page::kHost,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Close ports",
      .label_es = "Cerrar puertos",
      .help_en = "Removes the port mappings this game made.",
      .help_es = "Quita los reenvíos de puertos que hizo el juego."});
  out.push_back(Setting{
      .id = "net_public_address",
      .tab = Tab::kMultiplayer,
      .page = Page::kHost,
      .kind = Kind::kText,
      .apply = Apply::kRestart,
      .platforms = kWindows,
      .label_en = "Address friends use",
      .label_es = "Dirección para tus amigos",
      .help_en = "Your DNS name or public IP. Filled in when you open the ports and it is empty.",
      .help_es = "Tu nombre DNS o IP pública. Se llena al abrir los puertos si está vacía.",
      .max_length = 255});
  out.push_back(Setting{
      .id = "net_allow_peers",
      .tab = Tab::kMultiplayer,
      .page = Page::kHost,
      .kind = Kind::kText,
      .apply = Apply::kRestart,
      .platforms = kWindows,
      .label_en = "Friends allowed",
      .label_es = "Amigos permitidos",
      .help_en =
          "Friends' public IPs or DNS names, comma-separated. Empty: only your LAN and the Join addresses.",
      .help_es =
          "IP públicas o nombres DNS de tus amigos, separados por comas. Vacío: solo tu LAN y las "
          "direcciones de Unirse.",
      .max_length = 255});
  out.push_back(Setting{
      .id = "gow2_host_close_on_exit",
      .tab = Tab::kMultiplayer,
      .page = Page::kHost,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Close ports when the game exits",
      .label_es = "Cerrar puertos al salir del juego",
      .help_en = "Some routers ignore the time limit; this closes them when you quit.",
      .help_es = "Algunos routers ignoran el límite de tiempo; esto los cierra al salir."});
}

void AddSaves(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "gow2_save_dir",
      .tab = Tab::kSaves,
      .kind = Kind::kChoice,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .dynamic_choices = true,
      .label_en = "Save location",
      .label_es = "Ubicación de las partidas",
      .help_en = "Where saves, profile settings and achievements go. A cloud folder keeps them synced.",
      .help_es =
          "Dónde van las partidas, el perfil y los logros. Una carpeta en la nube las mantiene "
          "sincronizadas."});
  out.push_back(Setting{
      .id = "save_dir_browse",
      .tab = Tab::kSaves,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Choose a folder",
      .label_es = "Elegir una carpeta",
      .help_en = "Pick any folder, e.g. one inside OneDrive or Google Drive.",
      .help_es = "Elige cualquier carpeta, p. ej. una dentro de OneDrive o Google Drive."});
  out.push_back(Setting{
      .id = "gow2_save_dir_text",
      .tab = Tab::kSaves,
      .kind = Kind::kText,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .advanced = true,
      .label_en = "Folder path",
      .label_es = "Ruta de la carpeta",
      .help_en = "Typed path; %VARIABLES% work on Windows. Empty = default.",
      .help_es = "Ruta escrita; %VARIABLES% funcionan en Windows. Vacío = predeterminada.",
      .max_length = 260});
  out.push_back(Setting{
      .id = "save_dir_effective",
      .tab = Tab::kSaves,
      .kind = Kind::kInfo,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Saves are in",
      .label_es = "Las partidas están en",
      .help_en = "The folder this session uses.",
      .help_es = "La carpeta que usa esta sesión."});
  out.push_back(Setting{
      .id = "save_open",
      .tab = Tab::kSaves,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Open the saves folder",
      .label_es = "Abrir la carpeta de partidas",
      .help_en = "Opens it in File Explorer.",
      .help_es = "La abre en el Explorador de archivos."});
}

void AddAudio(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "gow2_audio_volume",
      .tab = Tab::kAudio,
      .kind = Kind::kNumber,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Volume",
      .label_es = "Volumen",
      .help_en = "Master volume of the game.",
      .help_es = "Volumen general del juego.",
      .min = 0,
      .max = 100,
      .step = 5,
      .decimals = 0,
      .unit = "%"});
  out.push_back(Setting{
      .id = "audio_mute",
      .tab = Tab::kAudio,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Mute",
      .label_es = "Silenciar",
      .help_en = "Silences the game.",
      .help_es = "Silencia el juego."});
  out.push_back(Setting{
      .id = "audio_maxqframes",
      .tab = Tab::kAudio,
      .kind = Kind::kNumber,
      .apply = Apply::kRestart,
      .platforms = kAllPlatforms,
      .advanced = true,
      .label_en = "Audio buffer",
      .label_es = "Búfer de audio",
      .help_en = "Raise it if the sound crackles; lower adds less delay.",
      .help_es = "Súbelo si el sonido chasquea; más bajo agrega menos retardo.",
      .min = 4,
      .max = 64,
      .step = 1,
      .decimals = 0});
}

void AddSystem(std::vector<Setting>& out) {
  out.push_back(Setting{
      .id = "update_status",
      .tab = Tab::kSystem,
      .kind = Kind::kInfo,
      .platforms = kWindows,
      .label_en = "Updates",
      .label_es = "Actualizaciones",
      .help_en = "Latest GitHub release status.",
      .help_es = "Estado de la última versión en GitHub."});
  out.push_back(Setting{
      .id = "gow2_update_check",
      .tab = Tab::kSystem,
      .kind = Kind::kToggle,
      .platforms = kWindows,
      .label_en = "Check for updates",
      .label_es = "Buscar actualizaciones",
      .help_en = "Checks GitHub at most twice a day. Sends no other data.",
      .help_es = "Consulta GitHub como máximo dos veces al día. No envía otros datos."});
  out.push_back(Setting{
      .id = "update_download",
      .tab = Tab::kSystem,
      .kind = Kind::kAction,
      .platforms = kWindows,
      .label_en = "Download",
      .label_es = "Descargar",
      .help_en = "Opens the latest release page in your browser.",
      .help_es = "Abre la página de la última versión en el navegador."});
  out.push_back(Setting{
      .id = "update_check_now",
      .tab = Tab::kSystem,
      .kind = Kind::kAction,
      .platforms = kWindows,
      .label_en = "Check now",
      .label_es = "Buscar ahora",
      .help_en = "Checks GitHub now when update checks are enabled.",
      .help_es = "Consulta GitHub ahora si la búsqueda de actualizaciones está activada."});
  out.push_back(Setting{
      .id = "update_dismiss",
      .tab = Tab::kSystem,
      .kind = Kind::kAction,
      .platforms = kWindows,
      .label_en = "Skip this version",
      .label_es = "Omitir esta versión",
      .help_en = "Hides the notice for this release; a newer release will show again.",
      .help_es = "Oculta el aviso de esta versión; una versión posterior volverá a avisar."});
  out.push_back(Setting{
      .id = "gow2_menu_language",
      .tab = Tab::kSystem,
      .kind = Kind::kChoice,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Menu language",
      .label_es = "Idioma del menú",
      .help_en = "Language of this menu.",
      .help_es = "Idioma de este menú.",
      .choices = {
          C("auto", "Automatic", "Automático"),
          C("en", "English"),
          C("es", "Español"),
      }});
  out.push_back(Setting{
      .id = "gow2_menu_advanced",
      .tab = Tab::kSystem,
      .kind = Kind::kToggle,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Show advanced options",
      .label_es = "Mostrar opciones avanzadas",
      .help_en = "Shows diagnostic and expert rows.",
      .help_es = "Muestra filas de diagnóstico y para expertos."});
  out.push_back(Setting{
      .id = "runtime_settings",
      .tab = Tab::kSystem,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "All runtime settings",
      .label_es = "Todos los ajustes del runtime",
      .help_en = "The runtime's own settings list (F4).",
      .help_es = "La lista de ajustes del propio runtime (F4)."});
  out.push_back(Setting{
      .id = "restart_game",
      .tab = Tab::kSystem,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kWindows,
      .label_en = "Restart the game",
      .label_es = "Reiniciar el juego",
      .help_en = "Saves nothing by itself: save first. Restarts with the same options.",
      .help_es = "No guarda nada por sí solo: guarda antes. Reinicia con las mismas opciones."});
  out.push_back(Setting{
      .id = "quit_game",
      .tab = Tab::kSystem,
      .kind = Kind::kAction,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Quit the game",
      .label_es = "Salir del juego",
      .help_en = "Closes the game; open it again to apply restart options.",
      .help_es = "Cierra el juego; ábrelo de nuevo para aplicar las opciones que piden reinicio."});
  out.push_back(Setting{
      .id = "config_path",
      .tab = Tab::kSystem,
      .kind = Kind::kInfo,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Settings file",
      .label_es = "Archivo de ajustes",
      .help_en = "Where this menu saves.",
      .help_es = "Dónde guarda este menú."});
  out.push_back(Setting{
      .id = "version",
      .tab = Tab::kSystem,
      .kind = Kind::kInfo,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Version",
      .label_es = "Versión",
      .help_en = "This build of the port.",
      .help_es = "Esta versión del port."});
  out.push_back(Setting{
      .id = "menu_shortcut",
      .tab = Tab::kSystem,
      .kind = Kind::kInfo,
      .apply = Apply::kNow,
      .platforms = kAllPlatforms,
      .label_en = "Open this menu",
      .label_es = "Abrir este menú",
#if defined(__ANDROID__)
      .help_en = "Gear, or Back + Start on a controller, or Options > Device Options in the game.",
      .help_es = "Engranaje, o Back + Start en el mando, u Opciones > Opciones del dispositivo en el juego."});
#else
      .help_en = "F10, or Back + Start on a controller, or Options > PC Options in the game.",
      .help_es = "F10, o Back + Start en el mando, u Opciones > Opciones de PC en el juego."});
#endif
}

std::vector<Setting> BuildCatalog() {
  std::vector<Setting> out;
  AddDisplay(out);
  AddGraphicsUpscaling(out);
  AddGraphicsPatches(out);
  AddGraphicsQuality(out);
  AddControls(out);
  AddKeybinds(out);
  AddTouch(out);
  AddMultiplayer(out);
  AddHost(out);
  AddSaves(out);
  AddAudio(out);
  AddSystem(out);
  return out;
}

}  // namespace

const std::vector<Setting>& Catalog() {
  static const std::vector<Setting> catalog = BuildCatalog();
  return catalog;
}

const Setting* FindSetting(std::string_view id) {
  for (const Setting& s : Catalog()) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

std::string_view Text(TextId id, Lang lang) {
  switch (id) {
    case TextId::kTitle:
#if defined(__ANDROID__)
      return Pick("DEVICE OPTIONS", "OPCIONES DEL DISPOSITIVO", lang);
#else
      return Pick("PC OPTIONS", "OPCIONES DE PC", lang);
#endif
    case TextId::kTabDisplay:
      return Pick("Display", "Pantalla", lang);
    case TextId::kTabGraphics:
      return Pick("Graphics", "Gráficos", lang);
    case TextId::kTabControls:
      return Pick("Controls", "Controles", lang);
    case TextId::kTabTouch:
      return Pick("Touch", "Táctil", lang);
    case TextId::kTabMultiplayer:
      return Pick("Multiplayer", "Multijugador", lang);
    case TextId::kTabSaves:
      return Pick("Saves", "Partidas", lang);
    case TextId::kTabAudio:
      return Pick("Audio", "Audio", lang);
    case TextId::kTabSystem:
      return Pick("System", "Sistema", lang);
    case TextId::kPageKeybinds:
      return Pick("Key bindings", "Asignación de teclas", lang);
    case TextId::kPageHost:
      return Pick("Host over the internet", "Alojar por internet", lang);
    case TextId::kOn:
      return Pick("On", "Sí", lang);
    case TextId::kOff:
      return Pick("Off", "No", lang);
    case TextId::kApplyNow:
      return Pick("Now", "Al instante", lang);
    case TextId::kApplyNextLevel:
      return Pick("Next level", "Próximo nivel", lang);
    case TextId::kApplyRestart:
      return Pick("Restart", "Al reiniciar", lang);
    case TextId::kRestartBanner:
      return Pick("Restart the game to apply: ", "Reinicia el juego para aplicar: ", lang);
    case TextId::kUnsavedTitle:
      return Pick("Save your changes?", "¿Guardar los cambios?", lang);
    case TextId::kUnsavedSave:
      return Pick("Save", "Guardar", lang);
    case TextId::kUnsavedDiscard:
      return Pick("Discard", "Descartar", lang);
    case TextId::kUnsavedCancel:
      return Pick("Cancel", "Cancelar", lang);
    case TextId::kSaved:
      return Pick("Saved to", "Guardado en", lang);
    case TextId::kSaveFailed:
      return Pick("Could not save", "No se pudo guardar", lang);
    case TextId::kPressAKey:
      return Pick("Press a key (Esc cancels)", "Pulsa una tecla (Esc cancela)", lang);
    case TextId::kHintChange:
      return Pick("Change", "Cambiar", lang);
    case TextId::kHintBack:
      return Pick("Back", "Atrás", lang);
    case TextId::kHintDefault:
      return Pick("Default", "Predeterminado", lang);
    case TextId::kHintSave:
      return Pick("Save", "Guardar", lang);
    case TextId::kHintTabs:
      return Pick("Tabs", "Pestañas", lang);
    case TextId::kHintClose:
      return Pick("Close", "Cerrar", lang);
    case TextId::kEditTextTitle:
      return Pick("Edit", "Editar", lang);
    case TextId::kEditTextOk:
      return Pick("OK", "Aceptar", lang);
    case TextId::kEditTextCancel:
      return Pick("Cancel", "Cancelar", lang);
  }
  return {};
}

std::string_view Label(const Setting& s, Lang lang) { return Pick(s.label_en, s.label_es, lang); }

std::string_view Help(const Setting& s, Lang lang) { return Pick(s.help_en, s.help_es, lang); }

std::string_view ChoiceLabel(const Choice& c, Lang lang) {
  return Pick(c.label_en, c.label_es, lang);
}

std::string_view TabName(Tab tab, Lang lang) {
  switch (tab) {
    case Tab::kDisplay:
      return Text(TextId::kTabDisplay, lang);
    case Tab::kGraphics:
      return Text(TextId::kTabGraphics, lang);
    case Tab::kControls:
      return Text(TextId::kTabControls, lang);
    case Tab::kTouch:
      return Text(TextId::kTabTouch, lang);
    case Tab::kMultiplayer:
      return Text(TextId::kTabMultiplayer, lang);
    case Tab::kSaves:
      return Text(TextId::kTabSaves, lang);
    case Tab::kAudio:
      return Text(TextId::kTabAudio, lang);
    case Tab::kSystem:
      return Text(TextId::kTabSystem, lang);
  }
  return {};
}

}  // namespace rex::glue::options
