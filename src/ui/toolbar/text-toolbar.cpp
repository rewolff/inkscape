/**
 * @file
 * Text aux toolbar
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 2001-2002 Ximian, Inc.
 * Copyright (C) 1999-2013 authors
 * Copyright (C) 2017 Tavmjong Bah
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glibmm/i18n.h>

#include "text-toolbar.h"

#include "desktop-style.h"
#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "widgets/ink-toggle-action.h"
#include "widgets/toolbox.h"
#include "inkscape.h"
#include "selection-chemistry.h"
#include "text-editing.h"
#include "verbs.h"

#include "libnrtype/font-lister.h"

#include "object/sp-flowdiv.h"
#include "object/sp-flowtext.h"
#include "object/sp-root.h"
#include "object/sp-text.h"
#include "object/sp-tspan.h"
#include "style.h"

#include "svg/css-ostringstream.h"

#include "ui/icon-names.h"
#include "ui/tools/text-tool.h"
#include "ui/widget/ink-select-one-action.h"
#include "ui/widget/unit-tracker.h"

#include "widgets/ege-adjustment-action.h"
#include "widgets/ink-comboboxentry-action.h"
#include "widgets/style-utils.h"

using Inkscape::DocumentUndo;
using Inkscape::UI::ToolboxFactory;
using Inkscape::UI::PrefPusher;
using Inkscape::Util::Unit;
using Inkscape::Util::Quantity;
using Inkscape::Util::unit_table;
using Inkscape::UI::Widget::UnitTracker;

//#define DEBUG_TEXT

//########################
//##    Text Toolbox    ##
//########################

// Functions for debugging:
#ifdef DEBUG_TEXT

static void       sp_print_font( SPStyle *query ) {

    bool family_set   = query->font_family.set;
    bool style_set    = query->font_style.set;
    bool fontspec_set = query->font_specification.set;

    std::cout << "    Family set? " << family_set
              << "    Style set? "  << style_set
              << "    FontSpec set? " << fontspec_set
              << std::endl;
    std::cout << "    Family: "
              << (query->font_family.value ? query->font_family.value : "No value")
              << "    Style: "    <<  query->font_style.computed
              << "    Weight: "   <<  query->font_weight.computed
              << "    FontSpec: "
              << (query->font_specification.value ? query->font_specification.value : "No value")
              << std::endl;
    std::cout << "    LineHeight: "    << query->line_height.computed
              << "    WordSpacing: "   << query->word_spacing.computed
              << "    LetterSpacing: " << query->letter_spacing.computed
              << std::endl;
}

static void       sp_print_fontweight( SPStyle *query ) {
    const gchar* names[] = {"100", "200", "300", "400", "500", "600", "700", "800", "900",
                            "NORMAL", "BOLD", "LIGHTER", "BOLDER", "Out of range"};
    // Missing book = 380
    int index = query->font_weight.computed;
    if( index < 0 || index > 13 ) index = 13;
    std::cout << "    Weight: " << names[ index ]
              << " (" << query->font_weight.computed << ")" << std::endl;

}

static void       sp_print_fontstyle( SPStyle *query ) {

    const gchar* names[] = {"NORMAL", "ITALIC", "OBLIQUE", "Out of range"};
    int index = query->font_style.computed;
    if( index < 0 || index > 3 ) index = 3;
    std::cout << "    Style:  " << names[ index ] << std::endl;

}
#endif

static void sp_text_toolbox_selection_changed(Inkscape::Selection */*selection*/, GObject *tbl, bool subselection = false);

// Font family
static void sp_text_fontfamily_value_changed( Ink_ComboBoxEntry_Action *act, GObject *tbl )
{
#ifdef DEBUG_TEXT
    std::cout << std::endl;
    std::cout << "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM" << std::endl;
    std::cout << "sp_text_fontfamily_value_changed: " << std::endl;
#endif

     // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
#ifdef DEBUG_TEXT
        std::cout << "sp_text_fontfamily_value_changed: frozen... return" << std::endl;
        std::cout << "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM\n" << std::endl;
#endif
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    Glib::ustring new_family = ink_comboboxentry_action_get_active_text( act );
    css_font_family_unquote( new_family ); // Remove quotes around font family names.

    // TODO: Think about how to handle handle multiple selections. While
    // the font-family may be the same for all, the styles might be different.
    // See: TextEdit::onApply() for example of looping over selected items.
    Inkscape::FontLister* fontlister = Inkscape::FontLister::get_instance();
#ifdef DEBUG_TEXT
    std::cout << "  Old family: " << fontlister->get_font_family() << std::endl;
    std::cout << "  New family: " << new_family << std::endl;
    std::cout << "  Old active: " << fontlister->get_font_family_row() << std::endl;
    std::cout << "  New active: " << act->active << std::endl;
#endif
    if( new_family.compare( fontlister->get_font_family() ) != 0 ) {
        // Changed font-family

        if( act->active == -1 ) {
            // New font-family, not in document, not on system (could be fallback list)
            fontlister->insert_font_family( new_family );
            act->active = 0; // New family is always at top of list.
        }

        fontlister->set_font_family( act->active );
        // active text set in sp_text_toolbox_selection_changed()

        SPCSSAttr *css = sp_repr_css_attr_new ();
        fontlister->fill_css( css );

        SPDesktop   *desktop    = SP_ACTIVE_DESKTOP;
        if( desktop->getSelection()->isEmpty() ) {
            // Update default
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            prefs->mergeStyle("/tools/text/style", css);
        } else {
            // If there is a selection, update
            sp_desktop_set_style (desktop, css, true, true); // Results in selection change called twice.
            DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_TEXT,
                               _("Text: Change font family"));
        }
        sp_repr_css_attr_unref (css);
    }

    // unfreeze
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );

#ifdef DEBUG_TEXT
    std::cout << "sp_text_toolbox_fontfamily_changes: exit"  << std::endl;
    std::cout << "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM" << std::endl;
    std::cout << std::endl;
#endif
}

// Font size
static void sp_text_fontsize_value_changed( Ink_ComboBoxEntry_Action *act, GObject *tbl )
{
     // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    gchar *text = ink_comboboxentry_action_get_active_text( act );
    gchar *endptr;
    gdouble size = g_strtod( text, &endptr );
    if (endptr == text) {  // Conversion failed, non-numeric input.
        g_warning( "Conversion of size text to double failed, input: %s\n", text );
        g_free( text );
        g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
        return;
    }
    g_free( text );

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    int max_size = prefs->getInt("/dialogs/textandfont/maxFontSize", 10000); // somewhat arbitrary, but text&font preview freezes with too huge fontsizes

    if (size > max_size)
        size = max_size;

    // Set css font size.
    SPCSSAttr *css = sp_repr_css_attr_new ();
    Inkscape::CSSOStringStream osfs;
    int unit = prefs->getInt("/options/font/unitType", SP_CSS_UNIT_PT);
    if (prefs->getBool("/options/font/textOutputPx", true)) {
        osfs << sp_style_css_size_units_to_px(size, unit) << sp_style_get_css_unit_string(SP_CSS_UNIT_PX);
    } else {
        osfs << size << sp_style_get_css_unit_string(unit);
    }
    sp_repr_css_set_property (css, "font-size", osfs.str().c_str());

    // Apply font size to selected objects.
    // Calling sp_desktop_set_style will result in a call to TextTool::_styleSet() which
    // will set the style on selected text inside the <text> element. If we want to set
    // the style on the outer <text> objects we need to bypass this call.
    bool outer = prefs->getInt("/tools/text/outer_style", false);
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    if (outer) {
        Inkscape::Selection *selection = desktop->getSelection();
        auto itemlist= selection->items();
        for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
            if (dynamic_cast<SPText *>(*i) || dynamic_cast<SPFlowtext *>(*i)) {
                SPItem *item = *i;

                // Scale by inverse of accumulated parent transform
                SPCSSAttr *css_set = sp_repr_css_attr_new();
                sp_repr_css_merge(css_set, css);
                Geom::Affine const local(item->i2doc_affine());
                double const ex(local.descrim());
                if ( (ex != 0.0) && (ex != 1.0) ) {
                    sp_css_attr_scale(css_set, 1/ex);
                }

                item->changeCSS(css_set,"style");

                sp_repr_css_attr_unref(css_set);
            }
        }
    } else {
        sp_desktop_set_style (desktop, css, true, true);
    }

    // If no selected objects, set default.
    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTNUMBERS);
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    } else {
        // Save for undo
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:size", SP_VERB_NONE,
                             _("Text: Change font size"));
    }

    sp_repr_css_attr_unref (css);

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

/*
 *  Font style
 */
static void sp_text_fontstyle_value_changed( Ink_ComboBoxEntry_Action *act, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    Glib::ustring new_style = ink_comboboxentry_action_get_active_text( act );

    Inkscape::FontLister* fontlister = Inkscape::FontLister::get_instance();

    if( new_style.compare( fontlister->get_font_style() ) != 0 ) {

        fontlister->set_font_style( new_style );
        // active text set in sp_text_toolbox_seletion_changed()

        SPCSSAttr *css = sp_repr_css_attr_new ();
        fontlister->fill_css( css );

        SPDesktop   *desktop    = SP_ACTIVE_DESKTOP;
        sp_desktop_set_style (desktop, css, true, true);


        // If no selected objects, set default.
        SPStyle query(SP_ACTIVE_DOCUMENT);
        int result_style =
            sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTSTYLE);
        if (result_style == QUERY_STYLE_NOTHING) {
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            prefs->mergeStyle("/tools/text/style", css);
        } else {
            // Save for undo
            DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_TEXT,
                               _("Text: Change font style"));
        }

        sp_repr_css_attr_unref (css);

    }

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

// Changes selection to only text outer elements.
static void sp_text_outer_style_changed( InkToggleAction*act, GObject *tbl )
{
    bool outer = gtk_toggle_action_get_active( GTK_TOGGLE_ACTION(act) );
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setInt("/tools/text/outer_style", outer);

    // Update widgets to reflect new state of Text Outer Style button.
    sp_text_toolbox_selection_changed( nullptr, tbl );
}

// Unset line height on selection's inner text objects (tspan, etc.).
static void sp_text_lineheight_unset_changed( InkToggleAction*act, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_unset_property(css, "line-height");

    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    sp_desktop_set_style (desktop, css);

    sp_repr_css_attr_unref(css);

    DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_TEXT,
                       _("Text: Unset line height."));

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

// Handles both Superscripts and Subscripts
static void sp_text_script_changed( InkToggleAction* act, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    // Called by Superscript or Subscript button?
    const gchar* name = gtk_action_get_name( GTK_ACTION( act ) );
    gint prop = (strcmp(name, "TextSuperscriptAction") == 0) ? 0 : 1;

#ifdef DEBUG_TEXT
    std::cout << "sp_text_script_changed: " << prop << std::endl;
#endif

    // Query baseline
    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_baseline = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_BASELINES);

    bool setSuper = false;
    bool setSub   = false;

    if (Inkscape::is_query_style_updateable(result_baseline)) {
        // If not set or mixed, turn on superscript or subscript
        if( prop == 0 ) {
            setSuper = true;
        } else {
            setSub = true;
        }
    } else {
        // Superscript
        gboolean superscriptSet = (query.baseline_shift.set &&
                                   query.baseline_shift.type == SP_BASELINE_SHIFT_LITERAL &&
                                   query.baseline_shift.literal == SP_CSS_BASELINE_SHIFT_SUPER );

        // Subscript
        gboolean subscriptSet = (query.baseline_shift.set &&
                                 query.baseline_shift.type == SP_BASELINE_SHIFT_LITERAL &&
                                 query.baseline_shift.literal == SP_CSS_BASELINE_SHIFT_SUB );

        setSuper = !superscriptSet && prop == 0;
        setSub   = !subscriptSet   && prop == 1;
    }

    // Set css properties
    SPCSSAttr *css = sp_repr_css_attr_new ();
    if( setSuper || setSub ) {
        // Openoffice 2.3 and Adobe use 58%, Microsoft Word 2002 uses 65%, LaTex about 70%.
        // 58% looks too small to me, especially if a superscript is placed on a superscript.
        // If you make a change here, consider making a change to baseline-shift amount
        // in style.cpp.
        sp_repr_css_set_property (css, "font-size", "65%");
    } else {
        sp_repr_css_set_property (css, "font-size", "");
    }
    if( setSuper ) {
        sp_repr_css_set_property (css, "baseline-shift", "super");
    } else if( setSub ) {
        sp_repr_css_set_property (css, "baseline-shift", "sub");
    } else {
        sp_repr_css_set_property (css, "baseline-shift", "baseline");
    }

    // Apply css to selected objects.
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    sp_desktop_set_style (desktop, css, true, false);

    // Save for undo
    if(result_baseline != QUERY_STYLE_NOTHING) {
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:script", SP_VERB_NONE,
                             _("Text: Change superscript or subscript"));
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_text_align_mode_changed( GObject *tbl, int mode )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setInt("/tools/text/align_mode", mode);

    SPDesktop *desktop = SP_ACTIVE_DESKTOP;

    // move the x of all texts to preserve the same bbox
    Inkscape::Selection *selection = desktop->getSelection();
    auto itemlist= selection->items();
    for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
        if (SP_IS_TEXT(*i)) {
            SPItem *item = *i;

            unsigned writing_mode = item->style->writing_mode.value;
            // below, variable names suggest horizontal move, but we check the writing direction
            // and move in the corresponding axis
            Geom::Dim2 axis;
            if (writing_mode == SP_CSS_WRITING_MODE_LR_TB || writing_mode == SP_CSS_WRITING_MODE_RL_TB) {
                axis = Geom::X;
            } else {
                axis = Geom::Y;
            }

            Geom::OptRect bbox = item->geometricBounds();
            if (!bbox)
                continue;
            double width = bbox->dimensions()[axis];
            // If you want to align within some frame, other than the text's own bbox, calculate
            // the left and right (or top and bottom for tb text) slacks of the text inside that
            // frame (currently unused)
            double left_slack = 0;
            double right_slack = 0;
            unsigned old_align = item->style->text_align.value;
            double move = 0;
            if (old_align == SP_CSS_TEXT_ALIGN_START || old_align == SP_CSS_TEXT_ALIGN_LEFT) {
                switch (mode) {
                    case 0:
                        move = -left_slack;
                        break;
                    case 1:
                        move = width/2 + (right_slack - left_slack)/2;
                        break;
                    case 2:
                        move = width + right_slack;
                        break;
                }
            } else if (old_align == SP_CSS_TEXT_ALIGN_CENTER) {
                switch (mode) {
                    case 0:
                        move = -width/2 - left_slack;
                        break;
                    case 1:
                        move = (right_slack - left_slack)/2;
                        break;
                    case 2:
                        move = width/2 + right_slack;
                        break;
                }
            } else if (old_align == SP_CSS_TEXT_ALIGN_END || old_align == SP_CSS_TEXT_ALIGN_RIGHT) {
                switch (mode) {
                    case 0:
                        move = -width - left_slack;
                        break;
                    case 1:
                        move = -width/2 + (right_slack - left_slack)/2;
                        break;
                    case 2:
                        move = right_slack;
                        break;
                }
            }
            Geom::Point XY = SP_TEXT(item)->attributes.firstXY();
            if (axis == Geom::X) {
                XY = XY + Geom::Point (move, 0);
            } else {
                XY = XY + Geom::Point (0, move);
            }
            SP_TEXT(item)->attributes.setFirstXY(XY);
            item->updateRepr();
            item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        }
    }

    SPCSSAttr *css = sp_repr_css_attr_new ();
    switch (mode)
    {
        case 0:
        {
            sp_repr_css_set_property (css, "text-anchor", "start");
            sp_repr_css_set_property (css, "text-align", "start");
            break;
        }
        case 1:
        {
            sp_repr_css_set_property (css, "text-anchor", "middle");
            sp_repr_css_set_property (css, "text-align", "center");
            break;
        }

        case 2:
        {
            sp_repr_css_set_property (css, "text-anchor", "end");
            sp_repr_css_set_property (css, "text-align", "end");
            break;
        }

        case 3:
        {
            sp_repr_css_set_property (css, "text-anchor", "start");
            sp_repr_css_set_property (css, "text-align", "justify");
            break;
        }
    }

    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTNUMBERS);

    // If querying returned nothing, update default style.
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    }

    sp_desktop_set_style (desktop, css, true, true);
    if (result_numbers != QUERY_STYLE_NOTHING)
    {
        DocumentUndo::done(SP_ACTIVE_DESKTOP->getDocument(), SP_VERB_CONTEXT_TEXT,
                       _("Text: Change alignment"));
    }
    sp_repr_css_attr_unref (css);

    gtk_widget_grab_focus (GTK_WIDGET(SP_ACTIVE_DESKTOP->canvas));

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static bool is_relative( Unit const *unit ) {
    return (unit->abbr == "" || unit->abbr == "em" || unit->abbr == "ex" || unit->abbr == "%");
}

// Set property for object, but unset all descendents
// Should probably be moved to desktop_style.cpp
static void recursively_set_properties( SPObject* object, SPCSSAttr *css ) {
    object->changeCSS (css, "style");

    SPCSSAttr *css_unset = sp_repr_css_attr_unset_all( css );
    std::vector<SPObject *> children = object->childList(false);
    for (auto i: children) {
        recursively_set_properties (i, css_unset);
    }
    sp_repr_css_attr_unref (css_unset);
}

// Apply line height changes (line-height value changed or line-height unit changed)
static void set_lineheight (SPCSSAttr *css) {

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool outer = prefs->getInt("/tools/text/outer_style", false);
    gint mode  = prefs->getInt("/tools/text/line_spacing_mode", 0);
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;

    // Calling sp_desktop_set_style will result in a call to TextTool::_styleSet() which
    // will set the style on selected text inside the <text> element. If we want to set
    // the style on the outer <text> objects we need to bypass this call.
    if ( mode == 3 && !outer ) {
        // This will call sp_te_apply_style via signal
        sp_desktop_set_style (desktop, css, true, true);
    } else {
        Inkscape::Selection *selection = desktop->getSelection();
        auto itemlist= selection->items();
        for (auto i: itemlist) {

            if (dynamic_cast<SPText *>(i) || dynamic_cast<SPFlowtext *>(i)) {
                SPItem *item = i;

                // Scale by inverse of accumulated parent transform
                SPCSSAttr *css_set = sp_repr_css_attr_new();
                sp_repr_css_merge(css_set, css);
                Geom::Affine const local(item->i2doc_affine());
                double const ex(local.descrim());
                if ( (ex != 0.0) && (ex != 1.0) ) {
                    sp_css_attr_scale(css_set, 1/ex);
                }

                if ( mode == 1 || mode == 2 || mode == 3) {  // Minimum, Even, or Adjustable w/ outer.
                    // We change only outer style
                    item->changeCSS(css_set,"style");
                } else {
                    // We change only inner style (Adaptive).
                    for (auto child: item->childList(false)) {
                        child->changeCSS(css_set,"style");
                    }
                }
            }
        }
    }
}

static void sp_text_lineheight_value_changed( GtkAdjustment *adj, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );


    // Get user selected unit and save as preference
    UnitTracker *tracker = reinterpret_cast<UnitTracker*>(g_object_get_data(tbl, "tracker"));
    Unit const *unit = tracker->getActiveUnit();
    g_return_if_fail(unit != nullptr);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();


    // This nonsense is to get SP_CSS_UNIT_xx value corresponding to unit so
    // we can save it (allows us to adjust line height value when unit changes).
    SPILength temp_length;
    Inkscape::CSSOStringStream temp_stream;
    temp_stream << 1 << unit->abbr;
    temp_length.read(temp_stream.str().c_str());
    prefs->setInt("/tools/text/lineheight/display_unit", temp_length.unit);
    g_object_set_data( tbl, "lineheight_unit", GINT_TO_POINTER(temp_length.unit));

    // Set css line height.
    SPCSSAttr *css = sp_repr_css_attr_new ();
    Inkscape::CSSOStringStream osfs;
    if ( is_relative(unit) ) {
        osfs << gtk_adjustment_get_value(adj) << unit->abbr;
    } else {
        // Inside SVG file, always use "px" for absolute units.
        osfs << Quantity::convert(gtk_adjustment_get_value(adj), unit, "px") << "px";
    }
    sp_repr_css_set_property (css, "line-height", osfs.str().c_str());

    // Internal function to set line-height which is spacing mode dependent.
    set_lineheight (css);

    // Only need to save for undo if a text item has been changed.
    Inkscape::Selection *selection = SP_ACTIVE_DESKTOP->getSelection();
    bool modmade = false;
    auto itemlist= selection->items();
    for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
        if (dynamic_cast<SPText *>(*i) || dynamic_cast<SPFlowtext *>(*i)) {
            modmade = true;
        }
    }

    // Save for undo
    if (modmade) {
        // Call ensureUpToDate() causes rebuild of text layout (with all proper style
        // cascading, etc.). For multi-line text with sodipodi::role="line", we must explicitly
        // save new <tspan> 'x' and 'y' attribute values by calling updateRepr().
        // Partial fix for bug #1590141.
        SP_ACTIVE_DESKTOP->getDocument()->ensureUpToDate();
        for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
            if (dynamic_cast<SPText *>(*i) || dynamic_cast<SPFlowtext *>(*i)) {
                (*i)->updateRepr();
            }
        }
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:line-height", SP_VERB_NONE,
                             _("Text: Change line-height"));
    }

    // If no selected objects, set default.
    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTNUMBERS);
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    }

    sp_repr_css_attr_unref (css);

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}


static void sp_text_lineheight_unit_changed( GObject *tbl, int /* Not Used */ )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    // Get old saved unit
    int old_unit = GPOINTER_TO_INT( g_object_get_data(tbl, "lineheight_unit"));

    // Get user selected unit and save as preference
    UnitTracker *tracker = reinterpret_cast<UnitTracker*>(g_object_get_data(tbl, "tracker"));
    Unit const *unit = tracker->getActiveUnit();
    g_return_if_fail(unit != nullptr);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // This nonsense is to get SP_CSS_UNIT_xx value corresponding to unit.
    SPILength temp_length;
    Inkscape::CSSOStringStream temp_stream;
    temp_stream << 1 << unit->abbr;
    temp_length.read(temp_stream.str().c_str());
    prefs->setInt("/tools/text/lineheight/display_unit", temp_length.unit);
    g_object_set_data( tbl, "lineheight_unit", GINT_TO_POINTER(temp_length.unit));

    // Read current line height value
    EgeAdjustmentAction *line_height_act =
        reinterpret_cast<EgeAdjustmentAction *>(g_object_get_data(tbl, "TextLineHeightAction"));
    GtkAdjustment *line_height_adj = ege_adjustment_action_get_adjustment( line_height_act );
    double line_height = gtk_adjustment_get_value(line_height_adj);

    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    Inkscape::Selection *selection = desktop->getSelection();
    auto itemlist = selection->items();

    // Convert between units
    if        ((unit->abbr == "" || unit->abbr == "em") &&
               (old_unit == SP_CSS_UNIT_NONE || old_unit == SP_CSS_UNIT_EM)) {
        // Do nothing
    } else if ((unit->abbr == "" || unit->abbr == "em") && old_unit == SP_CSS_UNIT_EX) {
        line_height *= 0.5;
    } else if ((unit->abbr) == "ex" && (old_unit == SP_CSS_UNIT_EM || old_unit == SP_CSS_UNIT_NONE) ) {
        line_height *= 2.0;
    } else if ((unit->abbr == "" || unit->abbr == "em") && old_unit == SP_CSS_UNIT_PERCENT) {
        line_height /= 100.0;
    } else if ((unit->abbr) == "%"  && (old_unit == SP_CSS_UNIT_EM || old_unit == SP_CSS_UNIT_NONE) ) {
        line_height *= 100;
    } else if ((unit->abbr) == "ex" && old_unit == SP_CSS_UNIT_PERCENT) {
        line_height /= 50.0;
    } else if ((unit->abbr) == "%"  && old_unit == SP_CSS_UNIT_EX) {
        line_height *= 50;
    } else if (is_relative(unit)) {
        // Convert absolute to relative... for the moment use average font-size
        double font_size = 0;
        int count = 0;
        for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
            if (SP_IS_TEXT (*i) || SP_IS_FLOWTEXT(*i)) {
                double doc_scale = Geom::Affine((*i)->i2dt_affine()).descrim();
                font_size += (*i)->style->font_size.computed * doc_scale;
                ++count;
            }
        }
        if (count > 0) {
            font_size /= count;
        } else {
            font_size = 20;
        }

        if (old_unit == SP_CSS_UNIT_NONE) old_unit = SP_CSS_UNIT_EM;
        line_height = Quantity::convert(line_height, sp_style_get_css_unit_string(old_unit), "px");

        if (font_size > 0) {
            line_height /= font_size;
        }
        if ((unit->abbr) == "%") {
            line_height *= 100;
        } else if ((unit->abbr) == "ex") {
            line_height *= 2;
        }
    } else if (old_unit==SP_CSS_UNIT_NONE || old_unit==SP_CSS_UNIT_PERCENT ||
               old_unit==SP_CSS_UNIT_EM   || old_unit==SP_CSS_UNIT_EX) {
        // Convert relative to absolute... for the moment use average font-size
        double font_size = 0;
        int count = 0;
        for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
            if (SP_IS_TEXT (*i) || SP_IS_FLOWTEXT (*i)) {
                double doc_scale = Geom::Affine((*i)->i2dt_affine()).descrim();
                font_size += (*i)->style->font_size.computed * doc_scale;
                ++count;
            }
        }
        if (count > 0) {
            font_size /= count;
        } else {
            font_size = 20;
        }

        if (old_unit == SP_CSS_UNIT_PERCENT) {
            line_height /= 100.0;
        } else if (old_unit == SP_CSS_UNIT_EX) {
            line_height /= 2.0;
        }
        line_height *= font_size;
        line_height = Quantity::convert(line_height, "px", unit);
    } else {
        // Convert between different absolute units (only used in GUI)
        line_height = Quantity::convert(line_height, sp_style_get_css_unit_string(old_unit), unit);
    }

    // Set css line height.
    SPCSSAttr *css = sp_repr_css_attr_new ();
    Inkscape::CSSOStringStream osfs;
    if ( is_relative(unit) ) {
        osfs << line_height << unit->abbr;
    } else {
        osfs << Quantity::convert(line_height, unit, "px") << "px";
    }
    sp_repr_css_set_property (css, "line-height", osfs.str().c_str());

    // Update GUI with line_height value.
    gtk_adjustment_set_value(line_height_adj, line_height);

    // Update "climb rate"  The custom action has a step property but no way to set it.
    if (unit->abbr == "%") {
        gtk_adjustment_set_step_increment (line_height_adj,  1.0);
        gtk_adjustment_set_page_increment (line_height_adj, 10.0);
    } else {
        gtk_adjustment_set_step_increment (line_height_adj,  0.1);
        gtk_adjustment_set_page_increment (line_height_adj,  1.0);
    }

    // Internal function to set line-height which is spacing mode dependent.
    set_lineheight (css);

    // Only need to save for undo if a text item has been changed.
    bool modmade = false;
    for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
        if (dynamic_cast<SPText *>(*i) || dynamic_cast<SPFlowtext *>(*i)) {
            modmade = true;
        }
    }

    // Save for undo
    if(modmade) {
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:line-height", SP_VERB_NONE,
                             _("Text: Change line-height unit"));
    }

    // If no selected objects, set default.
    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTNUMBERS);
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    }

    sp_repr_css_attr_unref (css);

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

//
static void sp_text_line_spacing_mode_changed( GObject *tbl, int mode )
{

    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setInt("/tools/text/line_spacing_mode", mode);

    SPDesktop *desktop = SP_ACTIVE_DESKTOP;

    // Note: only <text> and <flowRoot> text elements are in selection!
    // (No need to worry about <tspan>, <flowPara>, ...)
    Inkscape::Selection *selection = desktop->getSelection();
    std::vector<SPItem *> vec(selection->items().begin(), selection->items().end());

    for (auto i: vec) {

        // Only <text> and <flowRoot>, <flowRoot> => SPFlowtext
        if (dynamic_cast<SPText*>(i) || dynamic_cast<SPFlowtext*>(i)) {
            SPStyle* text_style = i->style;

            // Make list of <tspan>, <flowPara>, <flowSpan> children
            std::vector<SPObject *> children = i->childList(false);
            std::vector<SPItem *> children_item;
            for (auto j: children) {
                if (dynamic_cast<SPItem *>(j)) {
                    children_item.push_back( dynamic_cast<SPItem *>(j) );
                }
            }

            SPStyle tspans; // Also flowPara/flowSpan
            int result_numbers = sp_desktop_query_style_from_list (children_item, &tspans, QUERY_STYLE_PROPERTY_FONTNUMBERS);

            Inkscape::CSSOStringStream osfs;
            if (text_style->line_height.computed != 0 || text_style->line_height.normal) {

                if (mode != 1 || text_style->line_height.unit == SP_CSS_UNIT_NONE || text_style->line_height.normal) {
                    Glib::ustring line_height_string = text_style->line_height.write( SP_STYLE_FLAG_ALWAYS );
                    line_height_string.erase(0, 12); // erase 'line-height:'
                    osfs << line_height_string;
                } else {
                    // Convert to unitless value
                    double line_height_value = text_style->line_height.value;
                    // Percent values are stored as value/100;
                    if (text_style->line_height.unit == SP_CSS_UNIT_PERCENT) {
                        line_height_value *= 100;
                    }
                    osfs << sp_style_css_size_units_to_px( line_height_value,
                                                           text_style->line_height.unit,
                                                           text_style->font_size.computed) /
                        text_style->font_size.computed;
                }

            } else {

                if (mode != 1 || tspans.line_height.unit == SP_CSS_UNIT_NONE || tspans.line_height.normal) {
                    Glib::ustring line_height_string = tspans.line_height.write( SP_STYLE_FLAG_ALWAYS );
                    line_height_string.erase(0, 12); // erase 'line-height:'
                    osfs << line_height_string;
                } else {
                    // Convert to unitless value
                    double line_height_value = tspans.line_height.value;
                    // Percent values are stored as value/100;
                    if (tspans.line_height.unit == SP_CSS_UNIT_PERCENT) {
                        line_height_value *= 100;
                    }
                    osfs << sp_style_css_size_units_to_px( line_height_value,
                                                           tspans.line_height.unit,
                                                           tspans.font_size.computed) /
                        tspans.font_size.computed;
                }
            }

            SPCSSAttr *css_text  = sp_repr_css_attr_new();
            SPCSSAttr *css_tspan = sp_repr_css_attr_new();

            sp_repr_css_set_property (css_text, "line-height", osfs.str().c_str());

            switch (mode) {
                case 0: // Adaptive
                    // <text>: Zero text
                    sp_repr_css_set_property (css_text,  "line-height", "0");
                    // <tspan>:  Old text value.
                    sp_repr_css_set_property (css_tspan, "line-height", osfs.str().c_str());
                    break;

                case 1: // Minimum
                    // <text>: Set to old text (unitless) or if old text zero, set to old tspan.
                    sp_repr_css_set_property (css_text, "line-height", osfs.str().c_str());
                    // <tspan>: Unset
                    sp_repr_css_unset_property (css_tspan, "line-height");
                    break;

                case 2: // Even
                    // <text>: Set to old text or if old text zero, set to old tspan.
                    sp_repr_css_set_property (css_text, "line-height", osfs.str().c_str());
                    // <tspan>: Set to zero
                    sp_repr_css_set_property (css_tspan,  "line-height", "0");
                    break;

                case 3: // Adjustable
                    // Do nothing ☠
                    break;
            }

            if (mode != 3) {
                i->changeCSS (css_text, "style");
                for (auto j: children) {
                    recursively_set_properties (j, css_tspan);
                    //j->changeCSS (css_tspan, "style");
                }
            }

            sp_repr_css_attr_unref (css_text);
            sp_repr_css_attr_unref (css_tspan);
        }
    }

    // Set "Outer Style" toggle to match mode.
    InkToggleAction* outerStyle = INK_TOGGLE_ACTION(g_object_get_data (tbl, "TextOuterStyleAction"));

    switch (mode) {
        case 0: // Adaptive
            gtk_toggle_action_set_active( &(outerStyle->action), false );
            prefs->setInt("/tools/text/outer_style", false);
            break;

        case 1: // Minimum
        case 2: // Even
            gtk_toggle_action_set_active( &(outerStyle->action), true );
            prefs->setInt("/tools/text/outer_style", true);
            break;

        case 3: // Adjustable
            break;
    }

    // Outer style toggle set per mode so that line height widget should be enabled.
    GtkAction* lineHeightAction = GTK_ACTION( g_object_get_data( tbl, "TextLineHeightAction" ) );
    gtk_action_set_sensitive (lineHeightAction, true);

    // Update "climb rate"
    EgeAdjustmentAction *line_height_act =
        reinterpret_cast<EgeAdjustmentAction *>(g_object_get_data(tbl, "TextLineHeightAction"));
    GtkAdjustment *line_height_adj = ege_adjustment_action_get_adjustment( line_height_act );
    UnitTracker *tracker = reinterpret_cast<UnitTracker*>(g_object_get_data(tbl, "tracker"));
    Unit const *unit = tracker->getActiveUnit();

    if (unit->abbr == "%") {
        gtk_adjustment_set_step_increment (line_height_adj,  1.0);
        gtk_adjustment_set_page_increment (line_height_adj, 10.0);
    } else {
        gtk_adjustment_set_step_increment (line_height_adj,  0.1);
        gtk_adjustment_set_page_increment (line_height_adj,  1.0);
    }

    DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_TEXT,
                       _("Text: Change line spacing mode"));

    gtk_widget_grab_focus (GTK_WIDGET(desktop->canvas));

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_text_wordspacing_value_changed( GtkAdjustment *adj, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    // At the moment this handles only numerical values (i.e. no em unit).
    // Set css word-spacing
    SPCSSAttr *css = sp_repr_css_attr_new ();
    Inkscape::CSSOStringStream osfs;
    osfs << gtk_adjustment_get_value(adj) << "px"; // For now always use px
    sp_repr_css_set_property (css, "word-spacing", osfs.str().c_str());

    // Apply word-spacing to selected objects.
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    sp_desktop_set_style (desktop, css, true, false);

    // If no selected objects, set default.
    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTNUMBERS);
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    } else {
        // Save for undo
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:word-spacing", SP_VERB_NONE,
                             _("Text: Change word-spacing"));
    }

    sp_repr_css_attr_unref (css);

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_text_letterspacing_value_changed( GtkAdjustment *adj, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    // At the moment this handles only numerical values (i.e. no em unit).
    // Set css letter-spacing
    SPCSSAttr *css = sp_repr_css_attr_new ();
    Inkscape::CSSOStringStream osfs;
    osfs << gtk_adjustment_get_value(adj) << "px";  // For now always use px
    sp_repr_css_set_property (css, "letter-spacing", osfs.str().c_str());

    // Apply letter-spacing to selected objects.
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    sp_desktop_set_style (desktop, css, true, false);

    // If no selected objects, set default.
    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTNUMBERS);
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    }
    else
    {
        // Save for undo
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:letter-spacing", SP_VERB_NONE,
                             _("Text: Change letter-spacing"));
    }

    sp_repr_css_attr_unref (css);

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}


static void sp_text_dx_value_changed( GtkAdjustment *adj, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    gdouble new_dx = gtk_adjustment_get_value(adj);
    bool modmade = false;

    if( SP_IS_TEXT_CONTEXT((SP_ACTIVE_DESKTOP)->event_context) ) {
        Inkscape::UI::Tools::TextTool *const tc = SP_TEXT_CONTEXT((SP_ACTIVE_DESKTOP)->event_context);
        if( tc ) {
            unsigned char_index = -1;
            TextTagAttributes *attributes =
                text_tag_attributes_at_position( tc->text, std::min(tc->text_sel_start, tc->text_sel_end), &char_index );
            if( attributes ) {
                double old_dx = attributes->getDx( char_index );
                double delta_dx = new_dx - old_dx;
                sp_te_adjust_dx( tc->text, tc->text_sel_start, tc->text_sel_end, SP_ACTIVE_DESKTOP, delta_dx );
                modmade = true;
            }
        }
    }

    if(modmade) {
        // Save for undo
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:dx", SP_VERB_NONE,
                             _("Text: Change dx (kern)"));
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_text_dy_value_changed( GtkAdjustment *adj, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    gdouble new_dy = gtk_adjustment_get_value(adj);
    bool modmade = false;

    if( SP_IS_TEXT_CONTEXT((SP_ACTIVE_DESKTOP)->event_context) ) {
        Inkscape::UI::Tools::TextTool *const tc = SP_TEXT_CONTEXT((SP_ACTIVE_DESKTOP)->event_context);
        if( tc ) {
            unsigned char_index = -1;
            TextTagAttributes *attributes =
                text_tag_attributes_at_position( tc->text, std::min(tc->text_sel_start, tc->text_sel_end), &char_index );
            if( attributes ) {
                double old_dy = attributes->getDy( char_index );
                double delta_dy = new_dy - old_dy;
                sp_te_adjust_dy( tc->text, tc->text_sel_start, tc->text_sel_end, SP_ACTIVE_DESKTOP, delta_dy );
                modmade = true;
            }
        }
    }

    if(modmade) {
        // Save for undo
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:dy", SP_VERB_NONE,
                            _("Text: Change dy"));
    }

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_text_rotation_value_changed( GtkAdjustment *adj, GObject *tbl )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    gdouble new_degrees = gtk_adjustment_get_value(adj);

    bool modmade = false;
    if( SP_IS_TEXT_CONTEXT((SP_ACTIVE_DESKTOP)->event_context) ) {
        Inkscape::UI::Tools::TextTool *const tc = SP_TEXT_CONTEXT((SP_ACTIVE_DESKTOP)->event_context);
        if( tc ) {
            unsigned char_index = -1;
            TextTagAttributes *attributes =
                text_tag_attributes_at_position( tc->text, std::min(tc->text_sel_start, tc->text_sel_end), &char_index );
            if( attributes ) {
                double old_degrees = attributes->getRotate( char_index );
                double delta_deg = new_degrees - old_degrees;
                sp_te_adjust_rotation( tc->text, tc->text_sel_start, tc->text_sel_end, SP_ACTIVE_DESKTOP, delta_deg );
                modmade = true;
            }
        }
    }

    // Save for undo
    if(modmade) {
        DocumentUndo::maybeDone(SP_ACTIVE_DESKTOP->getDocument(), "ttb:rotate", SP_VERB_NONE,
                            _("Text: Change rotate"));
    }

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_writing_mode_changed( GObject* tbl, int mode )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    SPCSSAttr   *css        = sp_repr_css_attr_new ();
    switch (mode)
    {
        case 0:
        {
            sp_repr_css_set_property (css, "writing-mode", "lr-tb");
            break;
        }

        case 1:
        {
            sp_repr_css_set_property (css, "writing-mode", "tb-rl");
            break;
        }

        case 2:
        {
            sp_repr_css_set_property (css, "writing-mode", "vertical-lr");
            break;
        }
}

    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_WRITINGMODES);

    // If querying returned nothing, update default style.
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    }

    sp_desktop_set_style (SP_ACTIVE_DESKTOP, css, true, true);
    if(result_numbers != QUERY_STYLE_NOTHING)
    {
        DocumentUndo::done(SP_ACTIVE_DESKTOP->getDocument(), SP_VERB_CONTEXT_TEXT,
                       _("Text: Change writing mode"));
    }
    sp_repr_css_attr_unref (css);

    gtk_widget_grab_focus (GTK_WIDGET(SP_ACTIVE_DESKTOP->canvas));

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_text_orientation_changed( GObject* tbl, int mode )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    SPCSSAttr   *css        = sp_repr_css_attr_new ();
    switch (mode)
    {
        case 0:
        {
            sp_repr_css_set_property (css, "text-orientation", "auto");
            break;
        }

        case 1:
        {
            sp_repr_css_set_property (css, "text-orientation", "upright");
            break;
        }

        case 2:
        {
            sp_repr_css_set_property (css, "text-orientation", "sideways");
            break;
        }
    }

    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_WRITINGMODES);

    // If querying returned nothing, update default style.
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    }

    sp_desktop_set_style (SP_ACTIVE_DESKTOP, css, true, true);
    if(result_numbers != QUERY_STYLE_NOTHING)
    {
        DocumentUndo::done(SP_ACTIVE_DESKTOP->getDocument(), SP_VERB_CONTEXT_TEXT,
                       _("Text: Change orientation"));
    }
    sp_repr_css_attr_unref (css);

    gtk_widget_grab_focus (GTK_WIDGET(SP_ACTIVE_DESKTOP->canvas));

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_text_direction_changed( GObject *tbl, int mode )
{
    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    SPCSSAttr   *css        = sp_repr_css_attr_new ();
    switch (mode)
    {
        case 0:
        {
            sp_repr_css_set_property (css, "direction", "ltr");
            break;
        }

        case 1:
        {
            sp_repr_css_set_property (css, "direction", "rtl");
            break;
        }
    }

    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_numbers =
        sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_WRITINGMODES);

    // If querying returned nothing, update default style.
    if (result_numbers == QUERY_STYLE_NOTHING)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->mergeStyle("/tools/text/style", css);
    }

    sp_desktop_set_style (SP_ACTIVE_DESKTOP, css, true, true);
    if(result_numbers != QUERY_STYLE_NOTHING)
    {
        DocumentUndo::done(SP_ACTIVE_DESKTOP->getDocument(), SP_VERB_CONTEXT_TEXT,
                       _("Text: Change direction"));
    }
    sp_repr_css_attr_unref (css);

    gtk_widget_grab_focus (GTK_WIDGET(SP_ACTIVE_DESKTOP->canvas));

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

/*
 * Set the default list of font sizes, scaled to the users preferred unit
 */
static void sp_text_set_sizes(GtkListStore* model_size, int unit)
{
    gtk_list_store_clear(model_size);

    // List of font sizes for drop-down menu
    int sizes[] = {
        4, 6, 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 28,
        32, 36, 40, 48, 56, 64, 72, 144
    };

    // Array must be same length as SPCSSUnit in style.h
    float ratios[] = {1, 1, 1, 10, 4, 40, 100, 16, 8, 0.16};

    for( unsigned int i = 0; i < G_N_ELEMENTS(sizes); ++i ) {
        GtkTreeIter iter;
        Glib::ustring size = Glib::ustring::format(sizes[i] / (float)ratios[unit]);
        gtk_list_store_append( model_size, &iter );
        gtk_list_store_set( model_size, &iter, 0, size.c_str(), -1 );
    }
}


/*
 * This function sets up the text-tool tool-controls, setting the entry boxes
 * etc. to the values from the current selection or the default if no selection.
 * It is called whenever a text selection is changed, including stepping cursor
 * through text, or setting focus to text.
 */
static void sp_text_toolbox_selection_changed(Inkscape::Selection */*selection*/, GObject *tbl, bool subselection) // don't bother to update font list if subsel changed
{

#ifdef DEBUG_TEXT
    static int count = 0;
    ++count;
    std::cout << std::endl;
    std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
    std::cout << "sp_text_toolbox_selection_changed: start " << count << std::endl;

    Inkscape::Selection *selection = (SP_ACTIVE_DESKTOP)->getSelection();
    auto itemlist0= selection->items();
    for(auto i=itemlist0.begin();i!=itemlist0.end(); ++i) {
        const gchar* id = (*i)->getId();
        std::cout << "    " << id << std::endl;
    }
    Glib::ustring selected_text = sp_text_get_selected_text((SP_ACTIVE_DESKTOP)->event_context);
    std::cout << "  Selected text: |" << selected_text << "|" << std::endl;
#endif

    // quit if run by the _changed callbacks
    if (g_object_get_data(G_OBJECT(tbl), "freeze")) {
#ifdef DEBUG_TEXT
        std::cout << "    Frozen, returning" << std::endl;
        std::cout << "sp_text_toolbox_selection_changed: exit " << count << std::endl;
        std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
        std::cout << std::endl;
#endif
        return;
    }
    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

    Ink_ComboBoxEntry_Action* fontFamilyAction =
        INK_COMBOBOXENTRY_ACTION( g_object_get_data( tbl, "TextFontFamilyAction" ) );
    Ink_ComboBoxEntry_Action* fontStyleAction =
        INK_COMBOBOXENTRY_ACTION( g_object_get_data( tbl, "TextFontStyleAction"  ) );

    Inkscape::FontLister* fontlister = Inkscape::FontLister::get_instance();
    fontlister->update_font_list (SP_ACTIVE_DESKTOP->getDocument());
    fontlister->selection_update();

    // Update font list, but only if widget already created.
    if( fontFamilyAction->combobox != nullptr ) {
        ink_comboboxentry_action_set_active_text( fontFamilyAction, fontlister->get_font_family().c_str(), fontlister->get_font_family_row() );
        ink_comboboxentry_action_set_active_text( fontStyleAction,  fontlister->get_font_style().c_str()  );
    }

    // Only flowed text can be justified, only normal text can be kerned...
    // Find out if we have flowed text now so we can use it several places
    gboolean isFlow = false;
    auto itemlist= SP_ACTIVE_DESKTOP->getSelection()->items();
    for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
        // std::cout << "    " << ((*i)->getId()?(*i)->getId():"null") << std::endl;
        if( SP_IS_FLOWTEXT(*i)) {
            isFlow = true;
            // std::cout << "   Found flowed text" << std::endl;
            break;
        }
    }

    /*
     * Query from current selection:
     *   Font family (font-family)
     *   Style (font-weight, font-style, font-stretch, font-variant, font-align)
     *   Numbers (font-size, letter-spacing, word-spacing, line-height, text-anchor, writing-mode)
     *   Font specification (Inkscape private attribute)
     */
    SPStyle query(SP_ACTIVE_DOCUMENT);
    int result_family   = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTFAMILY);
    int result_style    = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTSTYLE);
    int result_baseline = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_BASELINES);
    int result_wmode    = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_WRITINGMODES);

    // Calling sp_desktop_query_style will result in a call to TextTool::_styleQueried().
    // This returns the style of the selected text inside the <text> element... which
    // is often the style of one or more <tspan>s. If we want the style of the outer
    // <text> objects then we need to bypass the call to TextTool::_styleQueried().
    // The desktop selection never includes the elements inside the <text> element.
    int result_numbers = 0;
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    SPDesktop   *desktop    = SP_ACTIVE_DESKTOP;
    bool outer = prefs->getInt("/tools/text/outer_style", false);
    if (outer) {
        Inkscape::Selection *selection = desktop->getSelection();
        std::vector<SPItem *> vec(selection->items().begin(), selection->items().end());
        result_numbers = sp_desktop_query_style_from_list (vec, &query, QUERY_STYLE_PROPERTY_FONTNUMBERS);
    } else {
        result_numbers = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_FONTNUMBERS);
    }

    /*
     * If no text in selection (querying returned nothing), read the style from
     * the /tools/text preferencess (default style for new texts). Return if
     * tool bar already set to these preferences.
     */
    if (result_family  == QUERY_STYLE_NOTHING ||
        result_style   == QUERY_STYLE_NOTHING ||
        result_numbers == QUERY_STYLE_NOTHING ||
        result_wmode   == QUERY_STYLE_NOTHING ) {
        // There are no texts in selection, read from preferences.
        query.readFromPrefs("/tools/text");
#ifdef DEBUG_TEXT
        std::cout << "    read style from prefs:" << std::endl;
        sp_print_font( &query );
#endif
        if (g_object_get_data(tbl, "text_style_from_prefs")) {
            // Do not reset the toolbar style from prefs if we already did it last time
            g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
#ifdef DEBUG_TEXT
            std::cout << "    text_style_from_prefs: toolbar already set" << std:: endl;
            std::cout << "sp_text_toolbox_selection_changed: exit " << count << std::endl;
            std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
            std::cout << std::endl;
#endif
            return;
        }

        // To ensure the value of the combobox is properly set on start-up, only mark
        // the prefs set if the combobox has already been constructed.
        if( fontFamilyAction->combobox != nullptr ) {
            g_object_set_data(tbl, "text_style_from_prefs", GINT_TO_POINTER(TRUE));
        }
    } else {
        g_object_set_data(tbl, "text_style_from_prefs", GINT_TO_POINTER(FALSE));
    }

    // If we have valid query data for text (font-family, font-specification) set toolbar accordingly.
    {
        // Size (average of text selected)
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        int unit = prefs->getInt("/options/font/unitType", SP_CSS_UNIT_PT);
        double size = sp_style_css_size_px_to_units(query.font_size.computed, unit);

        //gchar size_text[G_ASCII_DTOSTR_BUF_SIZE];
        //g_ascii_dtostr (size_text, sizeof (size_text), size);

        Inkscape::CSSOStringStream os;
        os << size;

        Ink_ComboBoxEntry_Action* fontSizeAction =
            INK_COMBOBOXENTRY_ACTION( g_object_get_data( tbl, "TextFontSizeAction" ) );

        // Freeze to ignore callbacks.
        //g_object_freeze_notify( G_OBJECT( fontSizeAction->combobox ) );
        sp_text_set_sizes(GTK_LIST_STORE(ink_comboboxentry_action_get_model(fontSizeAction)), unit);
        //g_object_thaw_notify( G_OBJECT( fontSizeAction->combobox ) );

        ink_comboboxentry_action_set_active_text( fontSizeAction, os.str().c_str() );

        Glib::ustring tooltip = Glib::ustring::format(_("Font size"), " (", sp_style_get_css_unit_string(unit), ")");
        ink_comboboxentry_action_set_tooltip ( fontSizeAction, tooltip.c_str());

        // Superscript
        gboolean superscriptSet =
            ((result_baseline == QUERY_STYLE_SINGLE || result_baseline == QUERY_STYLE_MULTIPLE_SAME ) &&
             query.baseline_shift.set &&
             query.baseline_shift.type == SP_BASELINE_SHIFT_LITERAL &&
             query.baseline_shift.literal == SP_CSS_BASELINE_SHIFT_SUPER );

        InkToggleAction* textSuperscriptAction = INK_TOGGLE_ACTION( g_object_get_data( tbl, "TextSuperscriptAction" ) );
        gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(textSuperscriptAction), superscriptSet );


        // Subscript
        gboolean subscriptSet =
            ((result_baseline == QUERY_STYLE_SINGLE || result_baseline == QUERY_STYLE_MULTIPLE_SAME ) &&
             query.baseline_shift.set &&
             query.baseline_shift.type == SP_BASELINE_SHIFT_LITERAL &&
             query.baseline_shift.literal == SP_CSS_BASELINE_SHIFT_SUB );

        InkToggleAction* textSubscriptAction = INK_TOGGLE_ACTION( g_object_get_data( tbl, "TextSubscriptAction" ) );
        gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(textSubscriptAction), subscriptSet );


        // Alignment
        InkSelectOneAction* textAlignAction =
            static_cast<InkSelectOneAction*>( g_object_get_data( tbl, "TextAlignAction" ) );

        // Note: SVG 1.1 doesn't include text-align, SVG 1.2 Tiny doesn't include text-align="justify"
        // text-align="justify" was a draft SVG 1.2 item (along with flowed text).
        // Only flowed text can be left and right justified at the same time.
        // Disable button if we don't have flowed text.

        Glib::RefPtr<Gtk::ListStore> store = textAlignAction->get_store();
        Gtk::TreeModel::Row row = *(store->get_iter("3"));  // Justify entry
        InkSelectOneActionColumns columns;
        row[columns.col_sensitive] = isFlow;

        int activeButton = 0;
        if (query.text_align.computed  == SP_CSS_TEXT_ALIGN_JUSTIFY)
        {
            activeButton = 3;
        } else {
            // This should take 'direction' into account
            if (query.text_anchor.computed == SP_CSS_TEXT_ANCHOR_START)  activeButton = 0;
            if (query.text_anchor.computed == SP_CSS_TEXT_ANCHOR_MIDDLE) activeButton = 1;
            if (query.text_anchor.computed == SP_CSS_TEXT_ANCHOR_END)    activeButton = 2;
        }
        textAlignAction->set_active( activeButton );

        // Line height (spacing) and line height unit
        double height;
        int line_height_unit = -1;
        if (query.line_height.normal) {
            height = Inkscape::Text::Layout::LINE_HEIGHT_NORMAL;
            line_height_unit = SP_CSS_UNIT_NONE;
        } else {
            height = query.line_height.value;
            line_height_unit = query.line_height.unit;
        }

        switch (line_height_unit) {
            case SP_CSS_UNIT_NONE:
            case SP_CSS_UNIT_EM:
            case SP_CSS_UNIT_EX:
                break;
            case SP_CSS_UNIT_PERCENT:
                height *= 100.0;  // Inkscape store % as fraction in .value
                break;
            case SP_CSS_UNIT_PX:
                // If unit is set to 'px', use the preferred display unit (if absolute).
                line_height_unit =
                    prefs->getInt("/tools/text/lineheight/display_unit", SP_CSS_UNIT_PT);
                // But not if preferred unit is relative
                if (line_height_unit != SP_CSS_UNIT_NONE &&
                    line_height_unit != SP_CSS_UNIT_EM &&
                    line_height_unit != SP_CSS_UNIT_EX &&
                    line_height_unit != SP_CSS_UNIT_PERCENT) {
                    height =
                        Quantity::convert(height, "px", sp_style_get_css_unit_string(line_height_unit));
                } else {
                    line_height_unit = SP_CSS_UNIT_PX;
                }
                break;
            default:
                // If unit has been set by an external program to something other than 'px', use
                // that unit.  But height is average of computed values (px) so we need to convert
                // back.
                height =
                    Quantity::convert(height, "px", sp_style_get_css_unit_string(line_height_unit));
        }

        GtkAction* lineHeightAction = GTK_ACTION( g_object_get_data( tbl, "TextLineHeightAction" ) );
        GtkAdjustment *lineHeightAdjustment =
            ege_adjustment_action_get_adjustment(EGE_ADJUSTMENT_ACTION( lineHeightAction ));
        gtk_adjustment_set_value( lineHeightAdjustment, height );

        // Update "climb rate"
        if (line_height_unit == SP_CSS_UNIT_PERCENT) {
            gtk_adjustment_set_step_increment (lineHeightAdjustment,  1.0);
            gtk_adjustment_set_page_increment (lineHeightAdjustment, 10.0);
        } else {
            gtk_adjustment_set_step_increment (lineHeightAdjustment,  0.1);
            gtk_adjustment_set_page_increment (lineHeightAdjustment,  1.0);
        }

        UnitTracker* tracker = reinterpret_cast<UnitTracker*>( g_object_get_data( tbl, "tracker" ) );
        if( line_height_unit == SP_CSS_UNIT_NONE ) {
            // Function 'sp_style_get_css_unit_string' returns 'px' for unit none.
            // We need to avoid this.
            tracker->setActiveUnitByAbbr("");
        } else {
            tracker->setActiveUnitByAbbr(sp_style_get_css_unit_string(line_height_unit));
        }
        // Save unit so we can do conversions between new/old units.
        g_object_set_data( tbl, "lineheight_unit", GINT_TO_POINTER(line_height_unit));

        // Enable and turn on only if selection includes an object with line height set.
        InkToggleAction* lineHeightUnset =
            INK_TOGGLE_ACTION( g_object_get_data( tbl, "TextLineHeightUnsetAction"));
        gtk_action_set_sensitive(GTK_ACTION(lineHeightUnset), query.line_height.set );
        gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(lineHeightUnset), query.line_height.set );


        // Line spacing mode: requires calculating mode for each <text> element and the <tspan>s within.
        Inkscape::Selection *selection = desktop->getSelection();
        std::vector<SPItem *> vec(selection->items().begin(), selection->items().end());
        int mode[4] = {0, 0, 0, 0};
        for (auto i: vec) {
            if (dynamic_cast<SPText*>(i) || dynamic_cast<SPFlowtext*>(i) ) {
                bool text_line_height_set = false;
                bool text_line_height_zero = false;
                bool text_line_height_has_units = false;
                bool tspan_line_height_all_unset    = true;
                bool tspan_line_height_all_zero     = true;
                bool tspan_line_height_all_non_zero = true;
                if (i->style && i->style->line_height.set) {
                    text_line_height_set = true;
                    if (i->style->line_height.computed == 0 && !(i->style->line_height.normal)) {
                        text_line_height_zero = true;
                    }
                    if (i->style->line_height.unit != SP_CSS_UNIT_NONE && !(i->style->line_height.normal)) {
                        text_line_height_has_units = true;
                    }
                }
                // TO DO: recursively check children
                std::vector<SPObject*> children = i->childList(false);
                for (auto j: children) {
                    if (dynamic_cast<SPTSpan*>(j) || dynamic_cast<SPFlowpara*>(j) || dynamic_cast<SPFlowtspan*>(j) ) {
                        if (j->style && j->style->line_height.set) {
                            tspan_line_height_all_unset = false;
                            if (j->style->line_height.computed != 0 || j->style->line_height.normal) {
                                tspan_line_height_all_zero = false;
                            } else {
                                tspan_line_height_all_non_zero = false;
                            }
                        }
                    }
                }

                if      ( text_line_height_zero && tspan_line_height_all_non_zero)   mode[0]++;
                else if (!text_line_height_has_units && tspan_line_height_all_unset) mode[1]++;
                else if ( text_line_height_has_units && tspan_line_height_all_unset) mode[3]++;
                else if ( tspan_line_height_all_zero )                               mode[2]++;
                else                                                                 mode[3]++;
            }
        }

        int activeButtonLS = 3;
        if (mode[0]  > 0 && mode[1] == 0 && mode[2] == 0 && mode[3] == 0) activeButtonLS = 0;
        if (mode[0] == 0 && mode[1]  > 0 && mode[2] == 0 && mode[3] == 0) activeButtonLS = 1;
        if (mode[0] == 0 && mode[1] == 0 && mode[2]  > 0 && mode[3] == 0) activeButtonLS = 2;
        // std::cout << "  modes: " << mode[0]
        //           << ", "<< mode[1]
        //           << ", "<< mode[2]
        //           << ", "<< mode[3] << std::endl;
        InkSelectOneAction* textLineSpacingAction =
            static_cast<InkSelectOneAction*>( g_object_get_data( tbl, "TextLineSpacingAction" ) );
        textLineSpacingAction->set_active( activeButtonLS );

        // Enable/disable line height widget based on mode and Outer Style toggle.
        if ( (activeButtonLS == 0 && outer)  ||   // Adaptive
             (activeButtonLS == 1 && !outer) ||   // Minimum
             (activeButtonLS == 2 && !outer)      // Even
            ) {
            gtk_action_set_sensitive (lineHeightAction, false);
        } else {
            gtk_action_set_sensitive (lineHeightAction, true);
        }

        // In Minimum and Adaptive modes, don't allow unit change (must remain unitless).
        InkSelectOneAction* textLineHeightUnitsAction =
            static_cast<InkSelectOneAction*>(g_object_get_data( tbl, "TextLineHeightUnitsAction") );
        if (activeButtonLS == 0 || (activeButtonLS == 1 && outer)) {
            textLineHeightUnitsAction->set_sensitive(false);
        } else {
            textLineHeightUnitsAction->set_sensitive(true);
        }

        // Word spacing
        double wordSpacing;
        if (query.word_spacing.normal) wordSpacing = 0.0;
        else wordSpacing = query.word_spacing.computed; // Assume no units (change in desktop-style.cpp)

        GtkAction* wordSpacingAction = GTK_ACTION( g_object_get_data( tbl, "TextWordSpacingAction" ) );
        GtkAdjustment *wordSpacingAdjustment =
            ege_adjustment_action_get_adjustment(EGE_ADJUSTMENT_ACTION( wordSpacingAction ));
        gtk_adjustment_set_value( wordSpacingAdjustment, wordSpacing );


        // Letter spacing
        double letterSpacing;
        if (query.letter_spacing.normal) letterSpacing = 0.0;
        else letterSpacing = query.letter_spacing.computed; // Assume no units (change in desktop-style.cpp)

        GtkAction* letterSpacingAction = GTK_ACTION( g_object_get_data( tbl, "TextLetterSpacingAction" ) );
        GtkAdjustment *letterSpacingAdjustment =
            ege_adjustment_action_get_adjustment(EGE_ADJUSTMENT_ACTION( letterSpacingAction ));
        gtk_adjustment_set_value( letterSpacingAdjustment, letterSpacing );


        // Writing mode
        int activeButton2 = 0;
        if (query.writing_mode.computed == SP_CSS_WRITING_MODE_LR_TB) activeButton2 = 0;
        if (query.writing_mode.computed == SP_CSS_WRITING_MODE_TB_RL) activeButton2 = 1;
        if (query.writing_mode.computed == SP_CSS_WRITING_MODE_TB_LR) activeButton2 = 2;

        InkSelectOneAction* writingModeAction =
            static_cast<InkSelectOneAction*>( g_object_get_data( tbl, "TextWritingModeAction" ) );
        writingModeAction->set_active( activeButton2 );

        // Orientation
        int activeButton3 = 0;
        if (query.text_orientation.computed == SP_CSS_TEXT_ORIENTATION_MIXED   ) activeButton3 = 0;
        if (query.text_orientation.computed == SP_CSS_TEXT_ORIENTATION_UPRIGHT ) activeButton3 = 1;
        if (query.text_orientation.computed == SP_CSS_TEXT_ORIENTATION_SIDEWAYS) activeButton3 = 2;

        InkSelectOneAction* textOrientationAction =
            static_cast<InkSelectOneAction*>( g_object_get_data( tbl, "TextOrientationAction" ) );
        textOrientationAction->set_active( activeButton3 );

        // Disable text orientation for horizontal text...
        textOrientationAction->set_sensitive( activeButton2 != 0 );

        // Direction
        int activeButton4 = 0;
        if (query.direction.computed == SP_CSS_DIRECTION_LTR ) activeButton4 = 0;
        if (query.direction.computed == SP_CSS_DIRECTION_RTL ) activeButton4 = 1;
        InkSelectOneAction* textDirectionAction =
            static_cast<InkSelectOneAction*>( g_object_get_data( tbl, "TextDirectionAction" ) );
        textDirectionAction->set_active( activeButton4 );
    }

#ifdef DEBUG_TEXT
    std::cout << "    GUI: fontfamily.value: "
              << (query.font_family.value ? query.font_family.value : "No value")
              << std::endl;
    std::cout << "    GUI: font_size.computed: "   << query.font_size.computed   << std::endl;
    std::cout << "    GUI: font_weight.computed: " << query.font_weight.computed << std::endl;
    std::cout << "    GUI: font_style.computed: "  << query.font_style.computed  << std::endl;
    std::cout << "    GUI: text_anchor.computed: " << query.text_anchor.computed << std::endl;
    std::cout << "    GUI: text_align.computed:  " << query.text_align.computed  << std::endl;
    std::cout << "    GUI: line_height.computed: " << query.line_height.computed
              << "  line_height.value: "    << query.line_height.value
              << "  line_height.unit: "     << query.line_height.unit  << std::endl;
    std::cout << "    GUI: word_spacing.computed: " << query.word_spacing.computed
              << "  word_spacing.value: "    << query.word_spacing.value
              << "  word_spacing.unit: "     << query.word_spacing.unit  << std::endl;
    std::cout << "    GUI: letter_spacing.computed: " << query.letter_spacing.computed
              << "  letter_spacing.value: "    << query.letter_spacing.value
              << "  letter_spacing.unit: "     << query.letter_spacing.unit  << std::endl;
    std::cout << "    GUI: writing_mode.computed: " << query.writing_mode.computed << std::endl;
#endif

    // Kerning (xshift), yshift, rotation.  NB: These are not CSS attributes.
    if( SP_IS_TEXT_CONTEXT((SP_ACTIVE_DESKTOP)->event_context) ) {
        Inkscape::UI::Tools::TextTool *const tc = SP_TEXT_CONTEXT((SP_ACTIVE_DESKTOP)->event_context);
        if( tc ) {
            unsigned char_index = -1;
            TextTagAttributes *attributes =
                text_tag_attributes_at_position( tc->text, std::min(tc->text_sel_start, tc->text_sel_end), &char_index );
            if( attributes ) {

                // Dx
                double dx = attributes->getDx( char_index );
                GtkAction* dxAction = GTK_ACTION( g_object_get_data( tbl, "TextDxAction" ));
                GtkAdjustment *dxAdjustment =
                    ege_adjustment_action_get_adjustment(EGE_ADJUSTMENT_ACTION( dxAction ));
                gtk_adjustment_set_value( dxAdjustment, dx );

                // Dy
                double dy = attributes->getDy( char_index );
                GtkAction* dyAction = GTK_ACTION( g_object_get_data( tbl, "TextDyAction" ));
                GtkAdjustment *dyAdjustment =
                    ege_adjustment_action_get_adjustment(EGE_ADJUSTMENT_ACTION( dyAction ));
                gtk_adjustment_set_value( dyAdjustment, dy );

                // Rotation
                double rotation = attributes->getRotate( char_index );
                /* SVG value is between 0 and 360 but we're using -180 to 180 in widget */
                if( rotation > 180.0 ) rotation -= 360.0;
                GtkAction* rotationAction = GTK_ACTION( g_object_get_data( tbl, "TextRotationAction" ));
                GtkAdjustment *rotationAdjustment =
                    ege_adjustment_action_get_adjustment(EGE_ADJUSTMENT_ACTION( rotationAction ));
                gtk_adjustment_set_value( rotationAdjustment, rotation );

#ifdef DEBUG_TEXT
                std::cout << "    GUI: Dx: " << dx << std::endl;
                std::cout << "    GUI: Dy: " << dy << std::endl;
                std::cout << "    GUI: Rotation: " << rotation << std::endl;
#endif
            }
        }
    }

    {
        // Set these here as we don't always have kerning/rotating attributes
        GtkAction* dxAction = GTK_ACTION( g_object_get_data( tbl, "TextDxAction" ));
        gtk_action_set_sensitive( GTK_ACTION(dxAction), !isFlow );

        GtkAction* dyAction = GTK_ACTION( g_object_get_data( tbl, "TextDyAction" ));
        gtk_action_set_sensitive( GTK_ACTION(dyAction), !isFlow );

        GtkAction* rotationAction = GTK_ACTION( g_object_get_data( tbl, "TextRotationAction" ));
        gtk_action_set_sensitive( GTK_ACTION(rotationAction), !isFlow );
    }

#ifdef DEBUG_TEXT
    std::cout << "sp_text_toolbox_selection_changed: exit " << count << std::endl;
    std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
    std::cout << std::endl;
#endif

    g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
}

static void sp_text_toolbox_selection_modified(Inkscape::Selection *selection, guint /*flags*/, GObject *tbl)
{
    sp_text_toolbox_selection_changed (selection, tbl);
}

static void
sp_text_toolbox_subselection_changed (gpointer /*tc*/, GObject *tbl)
{
    sp_text_toolbox_selection_changed (nullptr, tbl, true);
}

// TODO: possibly share with font-selector by moving most code to font-lister (passing family name)
static void sp_text_toolbox_select_cb( GtkEntry* entry, GtkEntryIconPosition /*position*/, GdkEvent /*event*/, gpointer /*data*/ ) {

  Glib::ustring family = gtk_entry_get_text ( entry );
  //std::cout << "text_toolbox_missing_font_cb: selecting: " << family << std::endl;

  // Get all items with matching font-family set (not inherited!).
  std::vector<SPItem*> selectList;

  SPDesktop *desktop = SP_ACTIVE_DESKTOP;
  SPDocument *document = desktop->getDocument();
  std::vector<SPItem*> x,y;
  std::vector<SPItem*> allList = get_all_items(x, document->getRoot(), desktop, false, false, true, y);
  for(std::vector<SPItem*>::const_reverse_iterator i=allList.rbegin();i!=allList.rend(); ++i){
      SPItem *item = *i;
    SPStyle *style = item->style;

    if (style) {

      Glib::ustring family_style;
      if (style->font_family.set) {
	family_style = style->font_family.value;
	//std::cout << " family style from font_family: " << family_style << std::endl;
      }
      else if (style->font_specification.set) {
	family_style = style->font_specification.value;
	//std::cout << " family style from font_spec: " << family_style << std::endl;
      }

      if (family_style.compare( family ) == 0 ) {
        //std::cout << "   found: " << item->getId() << std::endl;
	selectList.push_back(item);
      }
    }
  }

  // Update selection
  Inkscape::Selection *selection = desktop->getSelection();
  selection->clear();
  //std::cout << "   list length: " << g_slist_length ( selectList ) << std::endl;
  selection->setList(selectList);
}

static void text_toolbox_watch_ec(SPDesktop* dt, Inkscape::UI::Tools::ToolBase* ec, GObject* holder);

// Define all the "widgets" in the toolbar.
void sp_text_toolbox_prep(SPDesktop *desktop, GtkActionGroup* mainActions, GObject* holder)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    GtkIconSize secondarySize = ToolboxFactory::prefToSize("/toolbox/secondary", 1);

    /* Font family */
    {
        // Font list
        Inkscape::FontLister* fontlister = Inkscape::FontLister::get_instance();
        fontlister->update_font_list( SP_ACTIVE_DESKTOP->getDocument());
        Glib::RefPtr<Gtk::ListStore> store = fontlister->get_font_list();
        GtkListStore* model = store->gobj();

        Ink_ComboBoxEntry_Action* act =
            ink_comboboxentry_action_new( "TextFontFamilyAction",
                                          _("Font Family"),
                                          _("Select Font Family (Alt-X to access)"),
                                          nullptr,
                                          GTK_TREE_MODEL(model),
                                          -1,                // Entry width
                                          50,                // Extra list width
                                          (gpointer)font_lister_cell_data_func2, // Cell layout
                                          (gpointer)font_lister_separator_func2,
                                          GTK_WIDGET(desktop->canvas)); // Focus widget
        ink_comboboxentry_action_popup_enable( act ); // Enable entry completion

        gchar *const info = _("Select all text with this font-family");
        ink_comboboxentry_action_set_info( act, info ); // Show selection icon
        ink_comboboxentry_action_set_info_cb( act, (gpointer)sp_text_toolbox_select_cb );

        gchar *const warning = _("Font not found on system");
        ink_comboboxentry_action_set_warning( act, warning ); // Show icon w/ tooltip if font missing
        ink_comboboxentry_action_set_warning_cb( act, (gpointer)sp_text_toolbox_select_cb );

        //ink_comboboxentry_action_set_warning_callback( act, sp_text_fontfamily_select_all );
        ink_comboboxentry_action_set_altx_name( act, "altx-text" ); // Set Alt-X keyboard shortcut
        g_signal_connect( G_OBJECT(act), "changed", G_CALLBACK(sp_text_fontfamily_value_changed), holder );
        gtk_action_group_add_action( mainActions, GTK_ACTION(act) );
        g_object_set_data( holder, "TextFontFamilyAction", act );

        // Change style of drop-down from menu to list
        auto css_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(css_provider,
                                        "#TextFontFamilyAction_combobox {\n"
                                        "  -GtkComboBox-appears-as-list: true;\n"
                                        "}\n",
                                        -1, nullptr);

        auto screen = gdk_screen_get_default();
        gtk_style_context_add_provider_for_screen(screen,
                                                  GTK_STYLE_PROVIDER(css_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_USER);
    }

    /* Font size */
    {
        // List of font sizes for drop-down menu
        GtkListStore* model_size = gtk_list_store_new( 1, G_TYPE_STRING );
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        int unit = prefs->getInt("/options/font/unitType", SP_CSS_UNIT_PT);

        sp_text_set_sizes(model_size, unit);

        Glib::ustring tooltip = Glib::ustring::format(_("Font size"), " (", sp_style_get_css_unit_string(unit), ")");

        Ink_ComboBoxEntry_Action* act = ink_comboboxentry_action_new( "TextFontSizeAction",
                                                                      _("Font Size"),
                                                                      _(tooltip.c_str()),
                                                                      nullptr,
                                                                      GTK_TREE_MODEL(model_size),
                                                                      8,      // Width in characters
                                                                      0,      // Extra list width
                                                                      nullptr,   // Cell layout
                                                                      nullptr,   // Separator
                                                                      GTK_WIDGET(desktop->canvas)); // Focus widget

        g_signal_connect( G_OBJECT(act), "changed", G_CALLBACK(sp_text_fontsize_value_changed), holder );
        gtk_action_group_add_action( mainActions, GTK_ACTION(act) );
        g_object_set_data( holder, "TextFontSizeAction", act );
    }

    /* Font styles */
    {
        Inkscape::FontLister* fontlister = Inkscape::FontLister::get_instance();
        Glib::RefPtr<Gtk::ListStore> store = fontlister->get_style_list();
        GtkListStore* model_style = store->gobj();

        Ink_ComboBoxEntry_Action* act = ink_comboboxentry_action_new( "TextFontStyleAction",
                                                                      _("Font Style"),
                                                                      _("Font style"),
                                                                      nullptr,
                                                                      GTK_TREE_MODEL(model_style),
                                                                      12,     // Width in characters
                                                                      0,      // Extra list width
                                                                      nullptr,   // Cell layout
                                                                      nullptr,   // Separator
                                                                      GTK_WIDGET(desktop->canvas)); // Focus widget

        g_signal_connect( G_OBJECT(act), "changed", G_CALLBACK(sp_text_fontstyle_value_changed), holder );
        gtk_action_group_add_action( mainActions, GTK_ACTION(act) );
        g_object_set_data( holder, "TextFontStyleAction", act );
    }

    /* Style - Superscript */
    {
        InkToggleAction* act = ink_toggle_action_new( "TextSuperscriptAction",             // Name
                                                      _("Toggle Superscript"),             // Label
                                                      _("Toggle superscript"),             // Tooltip
                                                      "text_superscript",                  // Icon (inkId)
                                                      secondarySize );                     // Icon size
        gtk_action_group_add_action( mainActions, GTK_ACTION( act ) );
        g_signal_connect_after( G_OBJECT(act), "toggled", G_CALLBACK(sp_text_script_changed), holder );
        gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(act), prefs->getBool("/tools/text/super", false) );
        g_object_set_data( holder, "TextSuperscriptAction", act );
    }

    /* Style - Subscript */
    {
        InkToggleAction* act = ink_toggle_action_new( "TextSubscriptAction",             // Name
                                                      _("Toggle Subscript"),             // Label
                                                      _("Toggle subscript"),             // Tooltip
                                                      "text_subscript",                  // Icon (inkId)
                                                      secondarySize );                   // Icon size
        gtk_action_group_add_action( mainActions, GTK_ACTION( act ) );
        g_signal_connect_after( G_OBJECT(act), "toggled", G_CALLBACK(sp_text_script_changed), holder );
        gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(act), prefs->getBool("/tools/text/sub", false) );
        g_object_set_data( holder, "TextSubscriptAction", act );
    }

    /* Alignment */
    {
        InkSelectOneActionColumns columns;

        Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create(columns);

        Gtk::TreeModel::Row row;

        row = *(store->append());
        row[columns.col_label    ] = _("Align left");
        row[columns.col_tooltip  ] = _("Align left");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-justify-left");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Align center");
        row[columns.col_tooltip  ] = _("Align center");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-justify-center");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Align right");
        row[columns.col_tooltip  ] = _("Align right");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-justify-right");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Justify");
        row[columns.col_tooltip  ] = _("Justify (only flowed text)");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-justify-fill");
        row[columns.col_sensitive] = false;

        InkSelectOneAction* act =
            InkSelectOneAction::create( "TextAlignAction",   // Name
                                        _("Alignment"),      // Label
                                        _("Text alignment"), // Tooltip
                                        "Not Used",          // Icon
                                        store );             // Tree store
        act->use_radio( false );
        act->use_label( false );
        gint mode = prefs->getInt("/tools/text/align_mode", 0);
        act->set_active( mode );

        gtk_action_group_add_action( mainActions, GTK_ACTION( act->gobj() ));
        g_object_set_data( holder, "TextAlignAction", act );

        act->signal_changed().connect(sigc::bind<0>(sigc::ptr_fun(&sp_text_align_mode_changed), holder));
    }

    /* Writing mode (Horizontal, Vertical-LR, Vertical-RL) */
    {
        InkSelectOneActionColumns columns;

        Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create(columns);

        Gtk::TreeModel::Row row;

        row = *(store->append());
        row[columns.col_label    ] = _("Horizontal");
        row[columns.col_tooltip  ] = _("Horizontal text");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-text-direction-horizontal");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Vertical — RL");
        row[columns.col_tooltip  ] = _("Vertical text — lines: right to left");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-text-direction-vertical");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Vertical — LR");
        row[columns.col_tooltip  ] = _("Vertical text — lines: left to right");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-text-direction-vertical-lr");
        row[columns.col_sensitive] = true;

        InkSelectOneAction* act =
            InkSelectOneAction::create( "TextWritingModeAction", // Name
                                        _("Writing mode"),       // Label
                                        _("Block progression"),  // Tooltip
                                        "Not Used",              // Icon
                                        store );                 // Tree store
        act->use_radio( false );
        act->use_label( false );
        gint mode = prefs->getInt("/tools/text/writing_mode", 0);
        act->set_active( mode );

        gtk_action_group_add_action( mainActions, GTK_ACTION( act->gobj() ));
        g_object_set_data( holder, "TextWritingModeAction", act );

        act->signal_changed().connect(sigc::bind<0>(sigc::ptr_fun(&sp_writing_mode_changed), holder));
    }

    /* Text (glyph) orientation (Auto (mixed), Upright, Sideways) */
    {
        InkSelectOneActionColumns columns;

        Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create(columns);

        Gtk::TreeModel::Row row;

        row = *(store->append());
        row[columns.col_label    ] = _("Auto");
        row[columns.col_tooltip  ] = _("Auto glyph orientation");
        row[columns.col_icon     ] = INKSCAPE_ICON("text-orientation-auto");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Upright");
        row[columns.col_tooltip  ] = _("Upright glyph orientation");
        row[columns.col_icon     ] = INKSCAPE_ICON("text-orientation-upright");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Sideways");
        row[columns.col_tooltip  ] = _("Sideways glyph orientation");
        row[columns.col_icon     ] = INKSCAPE_ICON("text-orientation-sideways");
        row[columns.col_sensitive] = true;

        InkSelectOneAction* act =
            InkSelectOneAction::create( "TextOrientationAction",  // Name
                                        _("Text orientation"),    // Label
                                        _("Text (glyph) orientation in vertical text."),  // Tooltip
                                        "Not Used",               // Icon
                                        store );                  // List store
        act->use_radio( false );
        act->use_label( false );
        gint mode = prefs->getInt("/tools/text/text_orientation", 0);
        act->set_active( mode );

        gtk_action_group_add_action( mainActions, GTK_ACTION( act->gobj() ));
        g_object_set_data( holder, "TextOrientationAction", act );

        act->signal_changed().connect(sigc::bind<0>(sigc::ptr_fun(&sp_text_orientation_changed), holder));
    }


    // Text direction (predominant direction of horizontal text).
    {
        InkSelectOneActionColumns columns;

        Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create(columns);

        Gtk::TreeModel::Row row;

        row = *(store->append());
        row[columns.col_label    ] = _("LTR");
        row[columns.col_tooltip  ] = _("Left to right text");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-text-direction-horizontal");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("RTL");
        row[columns.col_tooltip  ] = _("Right to left text");
        row[columns.col_icon     ] = INKSCAPE_ICON("format-text-direction-r2l");
        row[columns.col_sensitive] = true;

        InkSelectOneAction* act =
            InkSelectOneAction::create( "TextDirectionAction",  // Name
                                        _("Text direction"),    // Label
                                        _("Text direction for normally horizontal text."),  // Tooltip
                                        "Not Used",               // Icon
                                        store );                  // List store
        act->use_radio( false );
        act->use_label( false );
        gint mode = prefs->getInt("/tools/text/text_direction", 0);
        act->set_active( mode );

        gtk_action_group_add_action( mainActions, GTK_ACTION( act->gobj() ));
        g_object_set_data( holder, "TextDirectionAction", act );

        act->signal_changed_after().connect(sigc::bind<0>(sigc::ptr_fun(&sp_text_direction_changed), holder));
    }

    /* Line height unit tracker */
    UnitTracker* tracker = new UnitTracker(Inkscape::Util::UNIT_TYPE_LINEAR);
    tracker->prependUnit(unit_table.getUnit("")); // No unit
    tracker->addUnit(unit_table.getUnit("%"));
    tracker->addUnit(unit_table.getUnit("em"));
    tracker->addUnit(unit_table.getUnit("ex"));
    tracker->setActiveUnit(unit_table.getUnit("%"));
    g_object_set_data( holder, "tracker", tracker );

    /* Line height */
    {
        // Drop down menu
        gchar const* labels[] = {_("Smaller spacing"), nullptr, nullptr, nullptr, nullptr, C_("Text tool", "Normal"), nullptr, nullptr, nullptr, nullptr, nullptr, _("Larger spacing")};
        gdouble values[] = { 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1,2, 1.3, 1.4, 1.5, 2.0};

        EgeAdjustmentAction *eact = create_adjustment_action(
            "TextLineHeightAction",               /* name */
            _("Line Height"),                     /* label */
            _("Line:"),                           /* short label */
            _("Spacing between baselines"),       /* tooltip */
            "/tools/text/lineheight",             /* preferences path */
            0.0,                                  /* default */
            GTK_WIDGET(desktop->canvas),          /* focusTarget */
            holder,                               /* dataKludge */
            FALSE,                                /* set alt-x keyboard shortcut? */
            nullptr,                                 /* altx_mark */
            0.0, 1000.0, 0.1, 1.0,                /* lower, upper, step (arrow up/down), page up/down */
            labels, values, G_N_ELEMENTS(labels), /* drop down menu */
            sp_text_lineheight_value_changed,     /* callback */
            nullptr, // tracker,                     /* unit tracker */
            0.1,                                  /* step (used?) */
            2,                                    /* digits to show */
            1.0                                   /* factor (multiplies default) */
            );
        //tracker->addAdjustment( ege_adjustment_action_get_adjustment(eact) );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
        g_object_set_data( holder, "TextLineHeightAction", eact );
        g_object_set( G_OBJECT(eact), "iconId", "text_line_spacing", NULL );
    }

    /* Line height units */
    {
        InkSelectOneAction* act = tracker->createAction( "TextLineHeightUnitsAction", _("Units"), ("") );
        gtk_action_group_add_action( mainActions, act->gobj() );
        act->signal_changed_after().connect(sigc::bind<0>(sigc::ptr_fun(&sp_text_lineheight_unit_changed), holder));
        g_object_set_data( holder, "TextLineHeightUnitsAction", act );
    }

    /* Line spacing mode */
    {
        InkSelectOneActionColumns columns;

        Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create(columns);

        Gtk::TreeModel::Row row;

        row = *(store->append());
        row[columns.col_label    ] = _("Adaptive");
        row[columns.col_tooltip  ] = _("Line spacing adapts to font size.");
        row[columns.col_icon     ] = INKSCAPE_ICON("text_line_spacing");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Minimum");
        row[columns.col_tooltip  ] = _("Line spacing adapts to fonts size with set minimum spacing.");
        row[columns.col_icon     ] = INKSCAPE_ICON("text_line_spacing");
        row[columns.col_sensitive] = true;
        row = *(store->append());
        row[columns.col_label    ] = _("Even");
        row[columns.col_tooltip  ] = _("Lines evenly spaced.");
        row[columns.col_icon     ] = INKSCAPE_ICON("text_line_spacing");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Adjustable ☠");
        row[columns.col_tooltip  ] = _("Line spacing fully adjustable");
        row[columns.col_icon     ] = INKSCAPE_ICON("text_line_spacing");
        row[columns.col_sensitive] = true;

        InkSelectOneAction* act =
            InkSelectOneAction::create( "TextLineSpacingAction", // Name
                                        _("Line Spacing Mode"),   // Label
                                        _("How should multiple baselines be spaced?\n Adaptive: Line spacing adapts to font size.\n Minimum: Like Adaptive, but with a set minimum.\n Even: Evenly spaced.\n Adjustable: No restrictions."), // Tooltip
                                        "Not Used",          // Icon
                                        store );             // Tree store
        act->use_radio( false );
        act->use_label( true );
        gint mode = prefs->getInt("/tools/text/line_spacing_mode", 0);
        act->set_active( mode );

        gtk_action_group_add_action( mainActions, GTK_ACTION( act->gobj() ));
        g_object_set_data( holder, "TextLineSpacingAction", act );

        act->signal_changed().connect(sigc::bind<0>(sigc::ptr_fun(&sp_text_line_spacing_mode_changed), holder));
    }

    /* Word spacing */
    {
        // Drop down menu
        gchar const* labels[] = {_("Negative spacing"), nullptr, nullptr, nullptr, C_("Text tool", "Normal"), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, _("Positive spacing")};
        gdouble values[] = {-2.0, -1.5, -1.0, -0.5, 0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0};

        EgeAdjustmentAction *eact = create_adjustment_action(
            "TextWordSpacingAction",              /* name */
            _("Word spacing"),                    /* label */
            _("Word:"),                           /* short label */
            _("Spacing between words (px)"),      /* tooltip */
            "/tools/text/wordspacing",            /* preferences path */
            0.0,                                  /* default */
            GTK_WIDGET(desktop->canvas),          /* focusTarget */
            holder,                               /* dataKludge */
            FALSE,                                /* set alt-x keyboard shortcut? */
            nullptr,                                 /* altx_mark */
            -100.0, 100.0, 0.01, 0.10,            /* lower, upper, step (arrow up/down), page up/down */
            labels, values, G_N_ELEMENTS(labels), /* drop down menu */
            sp_text_wordspacing_value_changed,    /* callback */
            nullptr,                                 /* unit tracker */
            0.1,                                  /* step (used?) */
            2,                                    /* digits to show */
            1.0                                   /* factor (multiplies default) */
            );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
        g_object_set_data( holder, "TextWordSpacingAction", eact );
        g_object_set( G_OBJECT(eact), "iconId", "text_word_spacing", NULL );
    }

    /* Letter spacing */
    {
        // Drop down menu
        gchar const* labels[] = {_("Negative spacing"), nullptr, nullptr, nullptr, C_("Text tool", "Normal"), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, _("Positive spacing")};
        gdouble values[] = {-2.0, -1.5, -1.0, -0.5, 0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0};

        EgeAdjustmentAction *eact = create_adjustment_action(
            "TextLetterSpacingAction",            /* name */
            _("Letter spacing"),                  /* label */
            _("Letter:"),                         /* short label */
            _("Spacing between letters (px)"),    /* tooltip */
            "/tools/text/letterspacing",          /* preferences path */
            0.0,                                  /* default */
            GTK_WIDGET(desktop->canvas),          /* focusTarget */
            holder,                               /* dataKludge */
            FALSE,                                /* set alt-x keyboard shortcut? */
            nullptr,                                 /* altx_mark */
            -100.0, 100.0, 0.01, 0.10,            /* lower, upper, step (arrow up/down), page up/down */
            labels, values, G_N_ELEMENTS(labels), /* drop down menu */
            sp_text_letterspacing_value_changed,  /* callback */
            nullptr,                                 /* unit tracker */
            0.1,                                  /* step (used?) */
            2,                                    /* digits to show */
            1.0                                   /* factor (multiplies default) */
            );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
        g_object_set_data( holder, "TextLetterSpacingAction", eact );
        g_object_set( G_OBJECT(eact), "iconId", "text_letter_spacing", NULL );
    }

    /* Character kerning (horizontal shift) */
    {
        // Drop down menu
        gchar const* labels[] = {   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr };
        gdouble values[]      = { -2.0, -1.5, -1.0, -0.5,   0,  0.5,  1.0,  1.5,  2.0, 2.5 };

        EgeAdjustmentAction *eact = create_adjustment_action(
            "TextDxAction",                       /* name */
            _("Kerning"),                         /* label */
            _("Kern:"),                           /* short label */
            _("Horizontal kerning (px)"),         /* tooltip */
            "/tools/text/dx",                     /* preferences path */
            0.0,                                  /* default */
            GTK_WIDGET(desktop->canvas),          /* focusTarget */
            holder,                               /* dataKludge */
            FALSE,                                /* set alt-x keyboard shortcut? */
            nullptr,                                 /* altx_mark */
            -100.0, 100.0, 0.01, 0.1,             /* lower, upper, step (arrow up/down), page up/down */
            labels, values, G_N_ELEMENTS(labels), /* drop down menu */
            sp_text_dx_value_changed,             /* callback */
            nullptr,                                 /* unit tracker */
            0.1,                                  /* step (used?) */
            2,                                    /* digits to show */
            1.0                                   /* factor (multiplies default) */
            );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
        g_object_set_data( holder, "TextDxAction", eact );
        g_object_set( G_OBJECT(eact), "iconId", "text_horz_kern", NULL );
    }

    /* Character vertical shift */
    {
        // Drop down menu
        gchar const* labels[] = {   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr };
        gdouble values[]      = { -2.0, -1.5, -1.0, -0.5,   0,  0.5,  1.0,  1.5,  2.0, 2.5 };

        EgeAdjustmentAction *eact = create_adjustment_action(
            "TextDyAction",                       /* name */
            _("Vertical Shift"),                  /* label */
            _("Vert:"),                           /* short label */
            _("Vertical shift (px)"),             /* tooltip */
            "/tools/text/dy",                     /* preferences path */
            0.0,                                  /* default */
            GTK_WIDGET(desktop->canvas),          /* focusTarget */
            holder,                               /* dataKludge */
            FALSE,                                /* set alt-x keyboard shortcut? */
            nullptr,                                 /* altx_mark */
            -100.0, 100.0, 0.01, 0.1,             /* lower, upper, step (arrow up/down), page up/down */
            labels, values, G_N_ELEMENTS(labels), /* drop down menu */
            sp_text_dy_value_changed,             /* callback */
            nullptr,                                 /* unit tracker */
            0.1,                                  /* step (used?) */
            2,                                    /* digits to show */
            1.0                                   /* factor (multiplies default) */
            );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
        g_object_set_data( holder, "TextDyAction", eact );
        g_object_set( G_OBJECT(eact), "iconId", "text_vert_kern", NULL );
    }

    /* Character rotation */
    {
        // Drop down menu
        gchar const* labels[] = {   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr,   nullptr };
        gdouble values[]      = { -90, -45, -30, -15,   0,  15,  30,  45,  90, 180 };

        EgeAdjustmentAction *eact = create_adjustment_action(
            "TextRotationAction",                 /* name */
            _("Letter rotation"),                 /* label */
            _("Rot:"),                            /* short label */
            _("Character rotation (degrees)"),    /* tooltip */
            "/tools/text/rotation",               /* preferences path */
            0.0,                                  /* default */
            GTK_WIDGET(desktop->canvas),          /* focusTarget */
            holder,                               /* dataKludge */
            FALSE,                                /* set alt-x keyboard shortcut? */
            nullptr,                                 /* altx_mark */
            -180.0, 180.0, 0.1, 1.0,              /* lower, upper, step (arrow up/down), page up/down */
            labels, values, G_N_ELEMENTS(labels), /* drop down menu */
            sp_text_rotation_value_changed,       /* callback */
            nullptr,                                 /* unit tracker */
            0.1,                                  /* step (used?) */
            2,                                    /* digits to show */
            1.0                                   /* factor (multiplies default) */
            );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
        g_object_set_data( holder, "TextRotationAction", eact );
        g_object_set( G_OBJECT(eact), "iconId", "text_rotation", NULL );
    }

    /* Text line height unset */
    {
        InkToggleAction* act = ink_toggle_action_new( "TextLineHeightUnsetAction",       // Name
                                                      _("Unset line height"),            // Label
                                                      _("If enabled, line height is set on part of selection. Click to unset."),
                                                      INKSCAPE_ICON("paint-unknown"),
                                                      secondarySize );                   // Icon size
        gtk_action_group_add_action( mainActions, GTK_ACTION( act ) );
        g_signal_connect_after( G_OBJECT(act), "toggled", G_CALLBACK(sp_text_lineheight_unset_changed), holder );
        gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(act), prefs->getBool("/tools/text/line_height_unset", false) );
        g_object_set_data( holder, "TextLineHeightUnsetAction", act );
    }

    /* Text outer style */
    {
        InkToggleAction* act = ink_toggle_action_new( "TextOuterStyleAction",            // Name
                                                      _("Show outer style"),             // Label
                                                      _("Show style of outermost text element. The 'font-size' and 'line-height' values of the outermost text element determine the minimum line spacing in the block."),
                                                      INKSCAPE_ICON("text_outer_style"),
                                                      secondarySize );                   // Icon size
        gtk_action_group_add_action( mainActions, GTK_ACTION( act ) );
        g_signal_connect_after( G_OBJECT(act), "toggled", G_CALLBACK(sp_text_outer_style_changed), holder );
        gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(act), prefs->getBool("/tools/text/outer_style", false) );
        g_object_set_data( holder, "TextOuterStyleAction", act );
    }

    // Is this necessary to call? Shouldn't hurt.
    sp_text_toolbox_selection_changed(desktop->getSelection(), holder);

    desktop->connectEventContextChanged(sigc::bind(sigc::ptr_fun(text_toolbox_watch_ec), holder));

    g_signal_connect( holder, "destroy", G_CALLBACK(purge_repr_listener), holder );

}

static void text_toolbox_watch_ec(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* ec, GObject* holder) {
    using sigc::connection;
    using sigc::bind;
    using sigc::ptr_fun;

    static connection c_selection_changed;
    static connection c_selection_modified;
    static connection c_subselection_changed;

    if (SP_IS_TEXT_CONTEXT(ec)) {
        // Watch selection

        // Ensure FontLister is updated here first.................. VVVVV
        c_selection_changed = desktop->getSelection()->connectChangedFirst(bind(ptr_fun(sp_text_toolbox_selection_changed), holder, false));
        c_selection_modified = desktop->getSelection()->connectModifiedFirst(bind(ptr_fun(sp_text_toolbox_selection_modified), holder));
        c_subselection_changed = desktop->connectToolSubselectionChanged(bind(ptr_fun(sp_text_toolbox_subselection_changed), holder));
    } else {
        if (c_selection_changed)
            c_selection_changed.disconnect();
        if (c_selection_modified)
            c_selection_modified.disconnect();
        if (c_subselection_changed)
            c_subselection_changed.disconnect();
    }
}
/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
