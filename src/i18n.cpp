#include "i18n.h"

#include "config.h"

// Default to all five languages compiled in if the build didn't say
// otherwise. Disabling one here trims its string table out of the firmware
// binary entirely (see the UBIBLION_LANG_* flags in platformio.ini).
#ifndef UBIBLION_LANG_EN
#define UBIBLION_LANG_EN 1
#endif
#ifndef UBIBLION_LANG_ES
#define UBIBLION_LANG_ES 1
#endif
#ifndef UBIBLION_LANG_FR
#define UBIBLION_LANG_FR 1
#endif
#ifndef UBIBLION_LANG_DE
#define UBIBLION_LANG_DE 1
#endif
#ifndef UBIBLION_LANG_IT
#define UBIBLION_LANG_IT 1
#endif
#ifndef UBIBLION_LANG_CA
#define UBIBLION_LANG_CA 1
#endif

#if !(UBIBLION_LANG_EN || UBIBLION_LANG_ES || UBIBLION_LANG_FR || UBIBLION_LANG_DE || \
      UBIBLION_LANG_IT || UBIBLION_LANG_CA)
#error "At least one UBIBLION_LANG_* must be enabled"
#endif

namespace I18n {
namespace {

// One PROGMEM string per Str value, per language. The index in each table
// must match the Str enum order exactly; STR_COUNT below is a compile-time
// tripwire for that.
#define UB_STR(name, text) const char name[] PROGMEM = text;

// Catches an entry being added to the Str enum without a matching string (or
// vice versa) at compile time, in whichever language happens to be enabled.
#define UB_CHECK_TABLE(table) \
  static_assert(sizeof(table) / sizeof(table[0]) == STR_COUNT, #table " size mismatch")

#if UBIBLION_LANG_EN
namespace en {
UB_STR(s00, "WiFi")
UB_STR(s01, "Press any button")
UB_STR(s02, "Indexing")
UB_STR(s03, "Preparing pages...")
UB_STR(s04, "BACK: cancel")
UB_STR(s05, "Off")
UB_STR(s06, "Network: ")
UB_STR(s07, "AP: ")
UB_STR(s08, "Key: ")
UB_STR(s09, "Indexing %u%%")
UB_STR(s10, "%u books, %uKB free")
UB_STR(s11, "BACK: turn off WiFi")
UB_STR(s12, "Could not index")
UB_STR(s13, "Unreadable book")
UB_STR(s14, "Connecting...")
UB_STR(s15, "Font: ")
UB_STR(s16, "Small")
UB_STR(s17, "Medium")
UB_STR(s18, "Large")
UB_STR(s19, "Full refresh: never")
UB_STR(s20, "Full refresh: ")
UB_STR(s21, "WiFi: turn off")
UB_STR(s22, "WiFi: turn on")
UB_STR(s23, "Language: ")
UB_STR(s24, "Reindex current book")
UB_STR(s25, "%uKB free")
UB_STR(s26, "Back")
UB_STR(s27, "Library")
UB_STR(s28, "Settings...")
UB_STR(s29, "Settings")
UB_STR(s30, "No books yet.")
UB_STR(s31, "Turn on WiFi to")
UB_STR(s32, "upload one.")
UB_STR(s33, "OK opens, long deletes")
UB_STR(s34, "OK to change")
UB_STR(s35, "Delete book")
UB_STR(s36, "Will delete:")
UB_STR(s37, "OK: confirm")
UB_STR(s38, "(empty page)")
UB_STR(s39, "%u pp")
}  // namespace en
const char* const TABLE_EN[] PROGMEM = {
    en::s00, en::s01, en::s02, en::s03, en::s04, en::s05, en::s06, en::s07, en::s08, en::s09,
    en::s10, en::s11, en::s12, en::s13, en::s14, en::s15, en::s16, en::s17, en::s18, en::s19,
    en::s20, en::s21, en::s22, en::s23, en::s24, en::s25, en::s26, en::s27, en::s28, en::s29,
    en::s30, en::s31, en::s32, en::s33, en::s34, en::s35, en::s36, en::s37, en::s38, en::s39,
};
UB_CHECK_TABLE(TABLE_EN);
#endif  // UBIBLION_LANG_EN

#if UBIBLION_LANG_ES
namespace es {
UB_STR(s00, "WiFi")
UB_STR(s01, "Pulsa cualquier botón")
UB_STR(s02, "Indexando")
UB_STR(s03, "Preparando páginas...")
UB_STR(s04, "BACK: cancelar")
UB_STR(s05, "Apagado")
UB_STR(s06, "Red: ")
UB_STR(s07, "AP: ")
UB_STR(s08, "Clave: ")
UB_STR(s09, "Indexando %u%%")
UB_STR(s10, "%u libros, %uKB libres")
UB_STR(s11, "BACK: apagar WiFi")
UB_STR(s12, "No se pudo indexar")
UB_STR(s13, "Libro ilegible")
UB_STR(s14, "Conectando...")
UB_STR(s15, "Fuente: ")
UB_STR(s16, "Pequeña")
UB_STR(s17, "Mediana")
UB_STR(s18, "Grande")
UB_STR(s19, "Refresco total: nunca")
UB_STR(s20, "Refresco total: ")
UB_STR(s21, "WiFi: apagar")
UB_STR(s22, "WiFi: encender")
UB_STR(s23, "Idioma: ")
UB_STR(s24, "Reindexar libro actual")
UB_STR(s25, "%uKB libres")
UB_STR(s26, "Volver")
UB_STR(s27, "Biblioteca")
UB_STR(s28, "Ajustes...")
UB_STR(s29, "Ajustes")
UB_STR(s30, "No hay libros.")
UB_STR(s31, "Enciende el WiFi")
UB_STR(s32, "para subir uno.")
UB_STR(s33, "OK abre, largo borra")
UB_STR(s34, "OK cambiar")
UB_STR(s35, "Borrar libro")
UB_STR(s36, "Se borrará:")
UB_STR(s37, "OK: confirmar")
UB_STR(s38, "(página vacía)")
UB_STR(s39, "%u pág.")
}  // namespace es
const char* const TABLE_ES[] PROGMEM = {
    es::s00, es::s01, es::s02, es::s03, es::s04, es::s05, es::s06, es::s07, es::s08, es::s09,
    es::s10, es::s11, es::s12, es::s13, es::s14, es::s15, es::s16, es::s17, es::s18, es::s19,
    es::s20, es::s21, es::s22, es::s23, es::s24, es::s25, es::s26, es::s27, es::s28, es::s29,
    es::s30, es::s31, es::s32, es::s33, es::s34, es::s35, es::s36, es::s37, es::s38, es::s39,
};
UB_CHECK_TABLE(TABLE_ES);
#endif  // UBIBLION_LANG_ES

#if UBIBLION_LANG_FR
namespace fr {
UB_STR(s00, "WiFi")
UB_STR(s01, "Appuie sur un bouton")
UB_STR(s02, "Indexation")
UB_STR(s03, "Préparation des pages...")
UB_STR(s04, "BACK : annuler")
UB_STR(s05, "Éteint")
UB_STR(s06, "Réseau : ")
UB_STR(s07, "AP : ")
UB_STR(s08, "Clé : ")
UB_STR(s09, "Indexation %u%%")
UB_STR(s10, "%u livres, %uKB libres")
UB_STR(s11, "BACK : éteindre le WiFi")
UB_STR(s12, "Indexation impossible")
UB_STR(s13, "Livre illisible")
UB_STR(s14, "Connexion...")
UB_STR(s15, "Police : ")
UB_STR(s16, "Petite")
UB_STR(s17, "Moyenne")
UB_STR(s18, "Grande")
UB_STR(s19, "Rafraîch. complet : jamais")
UB_STR(s20, "Rafraîch. complet : ")
UB_STR(s21, "WiFi : éteindre")
UB_STR(s22, "WiFi : activer")
UB_STR(s23, "Langue : ")
UB_STR(s24, "Réindexer le livre actuel")
UB_STR(s25, "%uKB libres")
UB_STR(s26, "Retour")
UB_STR(s27, "Bibliothèque")
UB_STR(s28, "Paramètres...")
UB_STR(s29, "Paramètres")
UB_STR(s30, "Aucun livre.")
UB_STR(s31, "Active le WiFi pour")
UB_STR(s32, "en envoyer un.")
UB_STR(s33, "OK ouvre, long efface")
UB_STR(s34, "OK pour changer")
UB_STR(s35, "Supprimer le livre")
UB_STR(s36, "Sera supprimé :")
UB_STR(s37, "OK : confirmer")
UB_STR(s38, "(page vide)")
UB_STR(s39, "%u p.")
}  // namespace fr
const char* const TABLE_FR[] PROGMEM = {
    fr::s00, fr::s01, fr::s02, fr::s03, fr::s04, fr::s05, fr::s06, fr::s07, fr::s08, fr::s09,
    fr::s10, fr::s11, fr::s12, fr::s13, fr::s14, fr::s15, fr::s16, fr::s17, fr::s18, fr::s19,
    fr::s20, fr::s21, fr::s22, fr::s23, fr::s24, fr::s25, fr::s26, fr::s27, fr::s28, fr::s29,
    fr::s30, fr::s31, fr::s32, fr::s33, fr::s34, fr::s35, fr::s36, fr::s37, fr::s38, fr::s39,
};
UB_CHECK_TABLE(TABLE_FR);
#endif  // UBIBLION_LANG_FR

#if UBIBLION_LANG_DE
namespace de {
UB_STR(s00, "WiFi")
UB_STR(s01, "Beliebige Taste drücken")
UB_STR(s02, "Indizierung")
UB_STR(s03, "Seiten werden vorbereitet...")
UB_STR(s04, "BACK: abbrechen")
UB_STR(s05, "Aus")
UB_STR(s06, "Netzwerk: ")
UB_STR(s07, "AP: ")
UB_STR(s08, "Schlüssel: ")
UB_STR(s09, "Indizierung %u%%")
UB_STR(s10, "%u Bücher, %uKB frei")
UB_STR(s11, "BACK: WLAN aus")
UB_STR(s12, "Indizierung fehlgeschlagen")
UB_STR(s13, "Buch nicht lesbar")
UB_STR(s14, "Verbindung...")
UB_STR(s15, "Schrift: ")
UB_STR(s16, "Klein")
UB_STR(s17, "Mittel")
UB_STR(s18, "Groß")
UB_STR(s19, "Vollaktualis.: nie")
UB_STR(s20, "Vollaktualis.: ")
UB_STR(s21, "WLAN: aus")
UB_STR(s22, "WLAN: ein")
UB_STR(s23, "Sprache: ")
UB_STR(s24, "Aktuelles Buch neu indizieren")
UB_STR(s25, "%uKB frei")
UB_STR(s26, "Zurück")
UB_STR(s27, "Bibliothek")
UB_STR(s28, "Einstellungen...")
UB_STR(s29, "Einstellungen")
UB_STR(s30, "Noch keine Bücher.")
UB_STR(s31, "WLAN einschalten,")
UB_STR(s32, "um eins hochzuladen.")
UB_STR(s33, "OK öffnet, lang löscht")
UB_STR(s34, "OK zum Ändern")
UB_STR(s35, "Buch löschen")
UB_STR(s36, "Wird gelöscht:")
UB_STR(s37, "OK: bestätigen")
UB_STR(s38, "(leere Seite)")
UB_STR(s39, "%u S.")
}  // namespace de
const char* const TABLE_DE[] PROGMEM = {
    de::s00, de::s01, de::s02, de::s03, de::s04, de::s05, de::s06, de::s07, de::s08, de::s09,
    de::s10, de::s11, de::s12, de::s13, de::s14, de::s15, de::s16, de::s17, de::s18, de::s19,
    de::s20, de::s21, de::s22, de::s23, de::s24, de::s25, de::s26, de::s27, de::s28, de::s29,
    de::s30, de::s31, de::s32, de::s33, de::s34, de::s35, de::s36, de::s37, de::s38, de::s39,
};
UB_CHECK_TABLE(TABLE_DE);
#endif  // UBIBLION_LANG_DE

#if UBIBLION_LANG_IT
namespace it {
UB_STR(s00, "WiFi")
UB_STR(s01, "Premi un tasto qualsiasi")
UB_STR(s02, "Indicizzazione")
UB_STR(s03, "Preparazione pagine...")
UB_STR(s04, "BACK: annulla")
UB_STR(s05, "Spento")
UB_STR(s06, "Rete: ")
UB_STR(s07, "AP: ")
UB_STR(s08, "Chiave: ")
UB_STR(s09, "Indicizzazione %u%%")
UB_STR(s10, "%u libri, %uKB liberi")
UB_STR(s11, "BACK: spegni WiFi")
UB_STR(s12, "Indicizzazione non riuscita")
UB_STR(s13, "Libro illeggibile")
UB_STR(s14, "Connessione...")
UB_STR(s15, "Carattere: ")
UB_STR(s16, "Piccolo")
UB_STR(s17, "Medio")
UB_STR(s18, "Grande")
UB_STR(s19, "Aggiorn. completo: mai")
UB_STR(s20, "Aggiorn. completo: ")
UB_STR(s21, "WiFi: spegni")
UB_STR(s22, "WiFi: accendi")
UB_STR(s23, "Lingua: ")
UB_STR(s24, "Reindicizza libro corrente")
UB_STR(s25, "%uKB liberi")
UB_STR(s26, "Indietro")
UB_STR(s27, "Libreria")
UB_STR(s28, "Impostazioni...")
UB_STR(s29, "Impostazioni")
UB_STR(s30, "Nessun libro.")
UB_STR(s31, "Attiva il WiFi per")
UB_STR(s32, "caricarne uno.")
UB_STR(s33, "OK apre, lungo elimina")
UB_STR(s34, "OK per cambiare")
UB_STR(s35, "Elimina libro")
UB_STR(s36, "Verrà eliminato:")
UB_STR(s37, "OK: conferma")
UB_STR(s38, "(pagina vuota)")
UB_STR(s39, "%u pag.")
}  // namespace it
const char* const TABLE_IT[] PROGMEM = {
    it::s00, it::s01, it::s02, it::s03, it::s04, it::s05, it::s06, it::s07, it::s08, it::s09,
    it::s10, it::s11, it::s12, it::s13, it::s14, it::s15, it::s16, it::s17, it::s18, it::s19,
    it::s20, it::s21, it::s22, it::s23, it::s24, it::s25, it::s26, it::s27, it::s28, it::s29,
    it::s30, it::s31, it::s32, it::s33, it::s34, it::s35, it::s36, it::s37, it::s38, it::s39,
};
UB_CHECK_TABLE(TABLE_IT);
#endif  // UBIBLION_LANG_IT

#if UBIBLION_LANG_CA
namespace ca {
UB_STR(s00, "WiFi")
UB_STR(s01, "Prem qualsevol botó")
UB_STR(s02, "Indexació")
UB_STR(s03, "Preparant pàgines...")
UB_STR(s04, "BACK: cancel·la")
UB_STR(s05, "Apagat")
UB_STR(s06, "Xarxa: ")
UB_STR(s07, "AP: ")
UB_STR(s08, "Clau: ")
UB_STR(s09, "Indexació %u%%")
UB_STR(s10, "%u llibres, %uKB lliures")
UB_STR(s11, "BACK: apaga el WiFi")
UB_STR(s12, "No s'ha pogut indexar")
UB_STR(s13, "Llibre il·legible")
UB_STR(s14, "Connectant...")
UB_STR(s15, "Lletra: ")
UB_STR(s16, "Petita")
UB_STR(s17, "Mitjana")
UB_STR(s18, "Gran")
UB_STR(s19, "Refresc total: mai")
UB_STR(s20, "Refresc total: ")
UB_STR(s21, "WiFi: apaga")
UB_STR(s22, "WiFi: engega")
UB_STR(s23, "Idioma: ")
UB_STR(s24, "Reindexa el llibre actual")
UB_STR(s25, "%uKB lliures")
UB_STR(s26, "Enrere")
UB_STR(s27, "Biblioteca")
UB_STR(s28, "Configuració...")
UB_STR(s29, "Configuració")
UB_STR(s30, "Encara no hi ha llibres.")
UB_STR(s31, "Engega el WiFi per")
UB_STR(s32, "pujar-ne un.")
UB_STR(s33, "OK obre, llarg elimina")
UB_STR(s34, "OK per canviar")
UB_STR(s35, "Elimina el llibre")
UB_STR(s36, "S'eliminarà:")
UB_STR(s37, "OK: confirma")
UB_STR(s38, "(pàgina buida)")
UB_STR(s39, "%u pàg.")
}  // namespace ca
const char* const TABLE_CA[] PROGMEM = {
    ca::s00, ca::s01, ca::s02, ca::s03, ca::s04, ca::s05, ca::s06, ca::s07, ca::s08, ca::s09,
    ca::s10, ca::s11, ca::s12, ca::s13, ca::s14, ca::s15, ca::s16, ca::s17, ca::s18, ca::s19,
    ca::s20, ca::s21, ca::s22, ca::s23, ca::s24, ca::s25, ca::s26, ca::s27, ca::s28, ca::s29,
    ca::s30, ca::s31, ca::s32, ca::s33, ca::s34, ca::s35, ca::s36, ca::s37, ca::s38, ca::s39,
};
UB_CHECK_TABLE(TABLE_CA);
#endif  // UBIBLION_LANG_CA

#undef UB_STR
#undef UB_CHECK_TABLE

struct LangEntry {
  const char* const* table;
  const char* code;
  const char* name;
};

const LangEntry kLangs[] = {
#if UBIBLION_LANG_EN
    {TABLE_EN, "en", "English"},
#endif
#if UBIBLION_LANG_ES
    {TABLE_ES, "es", "Español"},
#endif
#if UBIBLION_LANG_FR
    {TABLE_FR, "fr", "Français"},
#endif
#if UBIBLION_LANG_DE
    {TABLE_DE, "de", "Deutsch"},
#endif
#if UBIBLION_LANG_IT
    {TABLE_IT, "it", "Italiano"},
#endif
#if UBIBLION_LANG_CA
    {TABLE_CA, "ca", "Català"},
#endif
};
const uint8_t kLangCount = sizeof(kLangs) / sizeof(kLangs[0]);

uint8_t g_current = 0;

}  // namespace

void begin() {
  uint8_t saved = Config::get().lang;
  g_current = saved < kLangCount ? saved : 0;
}

const __FlashStringHelper* t(Str id) {
  const char* const* table = kLangs[g_current].table;
  const void* ptr = pgm_read_ptr(&table[id]);
  return reinterpret_cast<const __FlashStringHelper*>(ptr);
}

const char* fmt(Str id) { return reinterpret_cast<const char*>(t(id)); }

uint8_t languageCount() { return kLangCount; }
uint8_t currentLanguage() { return g_current; }

void setLanguage(uint8_t index) {
  g_current = index < kLangCount ? index : 0;
  Config::get().lang = g_current;
  Config::save();
}

const char* languageCode(uint8_t index) {
  return index < kLangCount ? kLangs[index].code : "en";
}

const char* languageName(uint8_t index) {
  return index < kLangCount ? kLangs[index].name : "English";
}

}  // namespace I18n
