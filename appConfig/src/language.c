#include "language.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct LanguageDefinition {
    const char* code;
    const char* texts[TEXT_COUNT];
} LanguageDefinition;

/*
 * To add a language, add its enum value in language.h and one complete entry
 * below. Keep the same TextId keys and preserve every %s placeholder.
 * Text is UTF-8; use U+XXXX escapes for non-ASCII characters so this source
 * remains portable between editors. The Windows UI supports the Latin-1
 * supplement used by Portuguese, French, and other Western European languages.
 */
static const LanguageDefinition languages[APP_LANGUAGE_COUNT] = {
    [APP_LANGUAGE_EN_US] = {
        .code = "EN-US",
        .texts = {
            [TEXT_APP_NAME] = "3SXW Configurator",
            [TEXT_TITLE] = "3SXW CONFIGURATOR",
            [TEXT_APPLY_NEXT_START] = "Changes take effect the next time the game starts.",
            [TEXT_INPUT_HELP] = "Mouse, arrows, Tab and Enter. Shift + arrows makes fine adjustments.",
            [TEXT_FULLSCREEN] = "Fullscreen",
            [TEXT_WINDOW_WIDTH] = "Window width",
            [TEXT_WINDOW_HEIGHT] = "Window height",
            [TEXT_ASPECT_RATIO] = "Aspect ratio",
            [TEXT_SCALE_MODE] = "Scale mode",
            [TEXT_FRAME_TIMING] = "Frame timing",
            [TEXT_BEZEL] = "16:9 bezel",
            [TEXT_SCANLINES] = "Scanlines",
            [TEXT_SCANLINE_OPACITY] = "Scanline intensity",
            [TEXT_PLAYERS_ABOVE_HUD] = "Players above HUD",
            [TEXT_SAVE] = "SAVE",
            [TEXT_DEFAULTS] = "DEFAULTS",
            [TEXT_CANCEL] = "CANCEL",
            [TEXT_STATUS_DIRTY] = "Changes have not been saved yet.",
            [TEXT_STATUS_LOADED] = "Configuration loaded.",
            [TEXT_STATUS_SAVED] = "Configuration saved successfully.",
            [TEXT_STATUS_SAVE_FAILED] = "The configuration could not be saved.",
            [TEXT_STATUS_LANGUAGE_CHANGED] = "Language changed to English.",
            [TEXT_UNSAVED_TITLE] = "Unsaved changes",
            [TEXT_UNSAVED_MESSAGE] = "There are unsaved changes. Do you want to discard them?",
            [TEXT_CONTINUE_EDITING] = "Continue editing",
            [TEXT_DISCARD] = "Discard",
            [TEXT_SAVE_ERROR_TITLE] = "Save error",
            [TEXT_FILE_PREFIX] = "File",
            [TEXT_ERR_LOCATE_APP_FOLDER] = "The configurator folder could not be located: %s",
            [TEXT_ERR_CREATE_CONFIG_PATH] = "The configuration path could not be created.",
            [TEXT_ERR_ACCESS_FOLDER] = "The folder could not be accessed:\n%s\n\n%s",
            [TEXT_ERR_CREATE_CONFIG_FILE_PATH] = "The config file path could not be created.",
            [TEXT_ERR_INVALID_READ_PARAMS] = "Invalid parameters for reading the configuration.",
            [TEXT_ERR_OPEN_FILE] = "The file could not be opened:\n%s\n\n%s",
            [TEXT_ERR_READ_FILE] = "An error occurred while reading:\n%s",
            [TEXT_ERR_INVALID_WRITE_PARAMS] = "Invalid parameters for writing the configuration.",
            [TEXT_ERR_READ_FOR_SAVE] = "The file could not be read:\n%s\n\n%s",
            [TEXT_ERR_CREATE_TEMP_PATH] = "The temporary file path could not be created.",
            [TEXT_ERR_WRITE_FILE] = "The file could not be written:\n%s\n\n%s",
            [TEXT_ERR_FINISH_WRITE] = "Writing the file could not be completed:\n%s",
            [TEXT_ERR_REPLACE_FILE] = "The file could not be replaced:\n%s\n\n%s",
            [TEXT_CONFIG_HEADER] = "# Portable 3SXW configuration",
        },
    },
    [APP_LANGUAGE_PT_BR] = {
        .code = "PT-BR",
        .texts = {
            [TEXT_APP_NAME] = "Configurador 3SXW",
            [TEXT_TITLE] = "CONFIGURADOR 3SXW",
            [TEXT_APPLY_NEXT_START] = "As altera\u00e7\u00f5es s\u00e3o aplicadas na pr\u00f3xima inicializa\u00e7\u00e3o do jogo.",
            [TEXT_INPUT_HELP] = "Mouse, setas, Tab e Enter. Shift + setas faz o ajuste fino.",
            [TEXT_FULLSCREEN] = "Tela cheia",
            [TEXT_WINDOW_WIDTH] = "Largura da janela",
            [TEXT_WINDOW_HEIGHT] = "Altura da janela",
            [TEXT_ASPECT_RATIO] = "Propor\u00e7\u00e3o da tela",
            [TEXT_SCALE_MODE] = "Modo de escala",
            [TEXT_FRAME_TIMING] = "Taxa de quadros",
            [TEXT_BEZEL] = "Moldura 16:9 (bezel)",
            [TEXT_SCANLINES] = "Scanlines",
            [TEXT_SCANLINE_OPACITY] = "Intensidade das scanlines",
            [TEXT_PLAYERS_ABOVE_HUD] = "Jogadores acima do HUD",
            [TEXT_SAVE] = "SALVAR",
            [TEXT_DEFAULTS] = "PADR\u00d5ES",
            [TEXT_CANCEL] = "CANCELAR",
            [TEXT_STATUS_DIRTY] = "Altera\u00e7\u00f5es ainda n\u00e3o foram salvas.",
            [TEXT_STATUS_LOADED] = "Configura\u00e7\u00e3o carregada.",
            [TEXT_STATUS_SAVED] = "Configura\u00e7\u00e3o salva com sucesso.",
            [TEXT_STATUS_SAVE_FAILED] = "N\u00e3o foi poss\u00edvel salvar a configura\u00e7\u00e3o.",
            [TEXT_STATUS_LANGUAGE_CHANGED] = "Idioma alterado para portugu\u00eas do Brasil.",
            [TEXT_UNSAVED_TITLE] = "Altera\u00e7\u00f5es n\u00e3o salvas",
            [TEXT_UNSAVED_MESSAGE] = "Existem altera\u00e7\u00f5es que ainda n\u00e3o foram salvas. Deseja descart\u00e1-las?",
            [TEXT_CONTINUE_EDITING] = "Continuar editando",
            [TEXT_DISCARD] = "Descartar",
            [TEXT_SAVE_ERROR_TITLE] = "Erro ao salvar",
            [TEXT_FILE_PREFIX] = "Arquivo",
            [TEXT_ERR_LOCATE_APP_FOLDER] = "N\u00e3o foi poss\u00edvel localizar a pasta do configurador: %s",
            [TEXT_ERR_CREATE_CONFIG_PATH] = "N\u00e3o foi poss\u00edvel criar o caminho da configura\u00e7\u00e3o.",
            [TEXT_ERR_ACCESS_FOLDER] = "N\u00e3o foi poss\u00edvel acessar a pasta:\n%s\n\n%s",
            [TEXT_ERR_CREATE_CONFIG_FILE_PATH] = "N\u00e3o foi poss\u00edvel criar o caminho do arquivo config.",
            [TEXT_ERR_INVALID_READ_PARAMS] = "Par\u00e2metros inv\u00e1lidos para leitura da configura\u00e7\u00e3o.",
            [TEXT_ERR_OPEN_FILE] = "N\u00e3o foi poss\u00edvel abrir:\n%s\n\n%s",
            [TEXT_ERR_READ_FILE] = "Ocorreu um erro durante a leitura de:\n%s",
            [TEXT_ERR_INVALID_WRITE_PARAMS] = "Par\u00e2metros inv\u00e1lidos para grava\u00e7\u00e3o da configura\u00e7\u00e3o.",
            [TEXT_ERR_READ_FOR_SAVE] = "N\u00e3o foi poss\u00edvel ler:\n%s\n\n%s",
            [TEXT_ERR_CREATE_TEMP_PATH] = "N\u00e3o foi poss\u00edvel criar o caminho tempor\u00e1rio.",
            [TEXT_ERR_WRITE_FILE] = "N\u00e3o foi poss\u00edvel gravar:\n%s\n\n%s",
            [TEXT_ERR_FINISH_WRITE] = "N\u00e3o foi poss\u00edvel concluir a grava\u00e7\u00e3o de:\n%s",
            [TEXT_ERR_REPLACE_FILE] = "N\u00e3o foi poss\u00edvel substituir:\n%s\n\n%s",
            [TEXT_CONFIG_HEADER] = "# Configura\u00e7\u00e3o port\u00e1til do 3SXW",
        },
    },
    [APP_LANGUAGE_FR_FRA] = {
        .code = "FR-FR",
        .texts = {
            [TEXT_APP_NAME] = "CONFIGURATION 3SXW",
            [TEXT_TITLE] = "CONFIGURATION 3SXW",
            [TEXT_APPLY_NEXT_START] = "Les changements seront appliqu\u00e9s au prochain lancement du jeu.",
            [TEXT_INPUT_HELP] = "Utilisez la souris, les fl\u00e8ches directionnelles, la touche Tab et la touche Entr\u00e9e. Maj + les fl\u00e8ches directionnelles permettent d'effectuer des ajustements pr\u00e9cis.",
            [TEXT_FULLSCREEN] = "Plein \u00e9cran",
            [TEXT_WINDOW_WIDTH] = "Largeur de la fen\u00eatre",
            [TEXT_WINDOW_HEIGHT] = "Hauteur de la fen\u00eatre",
            [TEXT_ASPECT_RATIO] = "Format de l'image",
            [TEXT_SCALE_MODE] = "Format de l'\u00e9cran",
            [TEXT_FRAME_TIMING] = "Fr\u00e9quence d'images",
            [TEXT_BEZEL] = "16:9 avec bordures",
            [TEXT_SCANLINES] = "Lignes de balayage",
            [TEXT_SCANLINE_OPACITY] = "Intensit\u00e9 des lignes de balayage",
            [TEXT_PLAYERS_ABOVE_HUD] = "Joueurs au-dessus du HUD",
            [TEXT_SAVE] = "SAUVEGARDER",
            [TEXT_DEFAULTS] = "R\u00c9GLAGES PAR D\u00c9FAUT",
            [TEXT_CANCEL] = "ANNULER",
            [TEXT_STATUS_DIRTY] = "Les changements n'ont pas encore \u00e9t\u00e9 sauvegard\u00e9s.",
            [TEXT_STATUS_LOADED] = "Configuration charg\u00e9e.",
            [TEXT_STATUS_SAVED] = "Configuration sauvegard\u00e9e avec succ\u00e8s.",
            [TEXT_STATUS_SAVE_FAILED] = "La configuration n'a pas pu \u00eatre sauvegard\u00e9e.",
            [TEXT_STATUS_LANGUAGE_CHANGED] = "Langue fran\u00e7aise s\u00e9lectionn\u00e9e.",
            [TEXT_UNSAVED_TITLE] = "Modifications non sauvegard\u00e9es",
            [TEXT_UNSAVED_MESSAGE] = "Certaines modifications n'ont pas \u00e9t\u00e9 sauvegard\u00e9es. \u00cates-vous s\u00fbr de vouloir continuer ?",
            [TEXT_CONTINUE_EDITING] = "Continuer la modification",
            [TEXT_DISCARD] = "IGNORER",
            [TEXT_SAVE_ERROR_TITLE] = "Erreur de sauvegarde",
            [TEXT_FILE_PREFIX] = "Fichier",
            [TEXT_ERR_LOCATE_APP_FOLDER] = "Le dossier de l'outil de configuration n'a pas pu \u00eatre trouv\u00e9 : %s",
            [TEXT_ERR_CREATE_CONFIG_PATH] = "Le chemin d'acc\u00e8s \u00e0 l'outil de configuration n'a pas pu \u00eatre cr\u00e9\u00e9.",
            [TEXT_ERR_ACCESS_FOLDER] = "Le dossier est inaccessible :\n%s\n\n%s",
            [TEXT_ERR_CREATE_CONFIG_FILE_PATH] = "Le chemin d'acc\u00e8s au fichier de configuration n'a pas pu \u00eatre cr\u00e9\u00e9.",
            [TEXT_ERR_INVALID_READ_PARAMS] = "Param\u00e8tres de lecture de la configuration non valides.",
            [TEXT_ERR_OPEN_FILE] = "Le fichier n'a pas pu \u00eatre ouvert :\n%s\n\n%s",
            [TEXT_ERR_READ_FILE] = "Une erreur est survenue pendant la lecture :\n%s",
            [TEXT_ERR_INVALID_WRITE_PARAMS] = "Param\u00e8tres d'enregistrement de la configuration non valides.",
            [TEXT_ERR_READ_FOR_SAVE] = "La lecture du fichier a \u00e9chou\u00e9 :\n%s\n\n%s",
            [TEXT_ERR_CREATE_TEMP_PATH] = "Le chemin d'acc\u00e8s au fichier temporaire n'a pas pu \u00eatre cr\u00e9\u00e9.",
            [TEXT_ERR_WRITE_FILE] = "Le fichier n'a pas pu \u00eatre cr\u00e9\u00e9 :\n%s\n\n%s",
            [TEXT_ERR_FINISH_WRITE] = "Impossible de finaliser l'\u00e9criture du fichier :\n%s",
            [TEXT_ERR_REPLACE_FILE] = "Le fichier ne peut pas \u00eatre remplac\u00e9 :\n%s\n\n%s",
            [TEXT_CONFIG_HEADER] = "# Configuration portable de 3SXW",
        },
    },
};

static AppLanguage current_language = APP_LANGUAGE_EN_US;

static bool language_is_valid(AppLanguage language) {
    return (int)language >= (int)APP_LANGUAGE_EN_US && (int)language < (int)APP_LANGUAGE_COUNT;
}

void Language_Init(void) {
    current_language = APP_LANGUAGE_EN_US;
}

void Language_SetCurrent(AppLanguage language) {
    if (language_is_valid(language)) {
        current_language = language;
    }
}

AppLanguage Language_GetCurrent(void) {
    return current_language;
}

int Language_GetCount(void) {
    return (int)APP_LANGUAGE_COUNT;
}

const char* Language_GetCode(AppLanguage language) {
    if (!language_is_valid(language) || languages[language].code == NULL) {
        return "";
    }

    return languages[language].code;
}

const char* Language_Get(TextId text) {
    if ((int)text < 0 || (int)text >= (int)TEXT_COUNT) {
        return "";
    }

    const char* value = languages[current_language].texts[text];

    if (value == NULL) {
        value = languages[APP_LANGUAGE_EN_US].texts[text];
    }

    return value != NULL ? value : "";
}
