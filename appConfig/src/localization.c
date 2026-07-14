#include "localization.h"

#include <stddef.h>

static AppLanguage current_language = APP_LANGUAGE_EN_US;

static const char* const translations[APP_LANGUAGE_COUNT][TEXT_COUNT] = {
    [APP_LANGUAGE_EN_US] = {
        [TEXT_APP_NAME] = "3SXW Configurator",
        [TEXT_TITLE] = "3SXW CONFIGURATOR",
        [TEXT_APPLY_NEXT_START] = "Changes take effect the next time the game starts.",
        [TEXT_INPUT_HELP] = "Mouse, arrows, Tab and Enter. Shift + arrows makes fine adjustments.",
        [TEXT_FULLSCREEN] = "Fullscreen",
        [TEXT_WINDOW_WIDTH] = "Window width",
        [TEXT_WINDOW_HEIGHT] = "Window height",
        [TEXT_SCALE_MODE] = "Scale mode",
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
    [APP_LANGUAGE_PT_BR] = {
        [TEXT_APP_NAME] = "Configurador 3SXW",
        [TEXT_TITLE] = "CONFIGURADOR 3SXW",
        [TEXT_APPLY_NEXT_START] = "As alteracoes sao aplicadas na proxima inicializacao do jogo.",
        [TEXT_INPUT_HELP] = "Mouse, setas, Tab e Enter. Shift + setas faz o ajuste fino.",
        [TEXT_FULLSCREEN] = "Tela cheia",
        [TEXT_WINDOW_WIDTH] = "Largura da janela",
        [TEXT_WINDOW_HEIGHT] = "Altura da janela",
        [TEXT_SCALE_MODE] = "Modo de escala",
        [TEXT_BEZEL] = "Moldura 16:9 (bezel)",
        [TEXT_SCANLINES] = "Scanlines",
        [TEXT_SCANLINE_OPACITY] = "Intensidade das scanlines",
        [TEXT_PLAYERS_ABOVE_HUD] = "Jogadores acima do HUD",
        [TEXT_SAVE] = "SALVAR",
        [TEXT_DEFAULTS] = "PADROES",
        [TEXT_CANCEL] = "CANCELAR",
        [TEXT_STATUS_DIRTY] = "Alteracoes ainda nao foram salvas.",
        [TEXT_STATUS_LOADED] = "Configuracao carregada.",
        [TEXT_STATUS_SAVED] = "Configuracao salva com sucesso.",
        [TEXT_STATUS_SAVE_FAILED] = "Nao foi possivel salvar a configuracao.",
        [TEXT_STATUS_LANGUAGE_CHANGED] = "Idioma alterado para portugues do Brasil.",
        [TEXT_UNSAVED_TITLE] = "Alteracoes nao salvas",
        [TEXT_UNSAVED_MESSAGE] = "Existem alteracoes que ainda nao foram salvas. Deseja descarta-las?",
        [TEXT_CONTINUE_EDITING] = "Continuar editando",
        [TEXT_DISCARD] = "Descartar",
        [TEXT_SAVE_ERROR_TITLE] = "Erro ao salvar",
        [TEXT_FILE_PREFIX] = "Arquivo",
        [TEXT_ERR_LOCATE_APP_FOLDER] = "Nao foi possivel localizar a pasta do configurador: %s",
        [TEXT_ERR_CREATE_CONFIG_PATH] = "Nao foi possivel criar o caminho da configuracao.",
        [TEXT_ERR_ACCESS_FOLDER] = "Nao foi possivel acessar a pasta:\n%s\n\n%s",
        [TEXT_ERR_CREATE_CONFIG_FILE_PATH] = "Nao foi possivel criar o caminho do arquivo config.",
        [TEXT_ERR_INVALID_READ_PARAMS] = "Parametros invalidos para leitura da configuracao.",
        [TEXT_ERR_OPEN_FILE] = "Nao foi possivel abrir:\n%s\n\n%s",
        [TEXT_ERR_READ_FILE] = "Ocorreu um erro durante a leitura de:\n%s",
        [TEXT_ERR_INVALID_WRITE_PARAMS] = "Parametros invalidos para gravacao da configuracao.",
        [TEXT_ERR_READ_FOR_SAVE] = "Nao foi possivel ler:\n%s\n\n%s",
        [TEXT_ERR_CREATE_TEMP_PATH] = "Nao foi possivel criar o caminho temporario.",
        [TEXT_ERR_WRITE_FILE] = "Nao foi possivel gravar:\n%s\n\n%s",
        [TEXT_ERR_FINISH_WRITE] = "Nao foi possivel concluir a gravacao de:\n%s",
        [TEXT_ERR_REPLACE_FILE] = "Nao foi possivel substituir:\n%s\n\n%s",
        [TEXT_CONFIG_HEADER] = "# Configuracao portatil do 3SXW",
    },
};

void Localization_Init(void) {
    current_language = APP_LANGUAGE_EN_US;
}

void Localization_SetLanguage(AppLanguage language) {
    if ((int)language >= (int)APP_LANGUAGE_EN_US && (int)language < (int)APP_LANGUAGE_COUNT) {
        current_language = language;
    }
}

AppLanguage Localization_GetLanguage(void) {
    return current_language;
}

const char* Localize(TextId text) {
    if ((int)text < 0 || (int)text >= (int)TEXT_COUNT) {
        return "";
    }

    const char* value = translations[current_language][text];
    return value != NULL ? value : "";
}
