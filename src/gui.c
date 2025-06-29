#include <gtk/gtk.h>
#include "fs_core.h"

// Variável global do IconView e do ListStore
GtkWidget *icon_view;
GtkListStore *list_store;

void update_ls() {
    gtk_list_store_clear(list_store);

    FileList file_list = fs_list_directory();

    for (size_t i = 0; i < file_list.count; ++i) {
        if (strcmp(file_list.entries[i].name, ".") == 0)
            continue;

        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);

        // Carrega o ícone do tema GTK
        GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
        GtkIconInfo *icon_info;
        GError *error = NULL;

        const char *icon_name = file_list.entries[i].type == TYPE_DIR ? "folder" : "text-x-generic";
        GdkPixbuf *pixbuf = NULL;

        icon_info = gtk_icon_theme_lookup_icon(icon_theme, icon_name, 48, 0);
        if (icon_info) {
            pixbuf = gtk_icon_info_load_icon(icon_info, &error);
            g_object_unref(icon_info);
        }

        gtk_list_store_set(list_store, &iter,
                           0, pixbuf,
                           1, file_list.entries[i].name,
                           2, file_list.entries[i].type,
                           -1);

        if (pixbuf)
            g_object_unref(pixbuf);
    }

    free(file_list.entries);
}

void on_item_properties(GtkMenuItem *menuitem, gpointer user_data) {
    const char *item_name = (const char *) user_data;
    Inode inode = fs_stat_item(item_name);

    if (inode.link_count == 0) {
        fprintf(stderr, "Item '%s' não encontrado.\n", item_name);
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Propriedades",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(menuitem))),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_OK", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(NULL);
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "Tipo: %s\nLinks: %u\nTamanho: %u bytes\nCriado em: %s\nAcessado em: %s\nModificado em: %s",
             (inode.type == TYPE_DIR) ? "Diretório" : "Arquivo",
             inode.link_count, inode.size,
             ctime(&inode.created), ctime(&inode.accessed), ctime(&inode.modified));
    gtk_label_set_text(GTK_LABEL(label), buffer);
    gtk_box_pack_start(GTK_BOX(content_area), label, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void on_delete_item(GtkMenuItem *menuitem, gpointer user_data) {
    const char *item_name = (const char *) user_data;
    if (fs_delete(item_name) == 0) {
        update_ls();
    } else {
        fprintf(stderr, "Erro ao deletar '%s'\n", item_name);
    }
    g_free(user_data);
}

void on_rename_item(GtkMenuItem *menuitem, gpointer user_data) {
    const char *old_name = (const char *) user_data;

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Renomear",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(menuitem))),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_OK", GTK_RESPONSE_ACCEPT,
                                                    "_Cancelar", GTK_RESPONSE_REJECT,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), old_name);
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (fs_rename(old_name, new_name) == 0) {
            update_ls();
        } else {
            fprintf(stderr, "Erro ao renomear '%s' para '%s'\n", old_name, new_name);
        }
    }

    gtk_widget_destroy(dialog);
    g_free(user_data);
}

void on_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
    GtkSelectionData *data, guint info, guint time, gpointer user_data) {

    g_print("[DEBUG] on_drag_data_received called\n");
    if (gtk_selection_data_get_length(data) <= 0) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    GtkTreePath *drop_path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(widget), x, y);

    gboolean success = FALSE;

    if (drop_path) {
        GtkTreeModel *model = gtk_icon_view_get_model(GTK_ICON_VIEW(widget));
        GtkTreeIter dest_iter;

        if (gtk_tree_model_get_iter(model, &dest_iter, drop_path)) {
            char *dest_name = NULL;
            int dest_type = -1;

            gtk_tree_model_get(model, &dest_iter,
                               1, &dest_name,
                               2, &dest_type,
                               -1);

            gchar *dragged_text = (gchar *)gtk_selection_data_get_text(data);

            if (dragged_text) {
                // Drag interno: mover item
                if (dest_type == TYPE_DIR) {
                    if (fs_move_item(dragged_text, dest_name) == 0) {
                        printf("Item '%s' movido para '%s'\n", dragged_text, dest_name);
                        success = TRUE;
                    } else {
                        fprintf(stderr, "Erro ao mover '%s' para '%s'\n", dragged_text, dest_name);
                    }
                } else {
                    fprintf(stderr, "Destino '%s' não é um diretório válido para mover.\n", dest_name);
                }
                g_free(dragged_text);
            } else {
                // Drag externo: arquivos URI(s)
                gchar **uris = gtk_selection_data_get_uris(data);
                if (uris) {
                    for (int i = 0; uris[i] != NULL; ++i) {
                        gchar *filename = g_filename_from_uri(uris[i], NULL, NULL);
                        if (filename) {
                            gchar *basename = g_path_get_basename(filename);

                            if (fs_change_directory(dest_name) == 0) {
                                if (fs_import_file(filename, basename) == 0) {
                                    printf("Importado com sucesso: %s\n", basename);
                                    success = TRUE;
                                } else {
                                    fprintf(stderr, "Falha ao importar: %s\n", basename);
                                }
                                fs_change_directory("..");
                            } else {
                                fprintf(stderr, "Erro ao acessar diretório '%s' para importar\n", dest_name);
                            }

                            g_free(basename);
                            g_free(filename);
                        }
                    }
                    g_strfreev(uris);
                }
            }

            g_free(dest_name);
        }

        gtk_tree_path_free(drop_path);
    } else {
        // Drop em espaço vazio: importar arquivos externos no diretório atual
        gchar **uris = gtk_selection_data_get_uris(data);
        if (uris) {
            for (int i = 0; uris[i] != NULL; ++i) {
                gchar *filename = g_filename_from_uri(uris[i], NULL, NULL);
                if (filename) {
                    gchar *basename = g_path_get_basename(filename);
                    if (fs_import_file(filename, basename) == 0) {
                        printf("Importado com sucesso: %s\n", basename);
                        success = TRUE;
                    } else {
                        fprintf(stderr, "Falha ao importar: %s\n", basename);
                    }
                    g_free(basename);
                    g_free(filename);
                }
            }
            g_strfreev(uris);
        }
    }

    update_ls();
    gtk_drag_finish(context, success, FALSE, time);
}
void on_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer user_data) {
    g_print("[DEBUG] drag-begin\n");
}
static void on_drag_data_get(GtkWidget *widget, GdkDragContext *context,
                             GtkSelectionData *selection_data, guint info, guint time, gpointer user_data) {
    g_print("[DEBUG] on_drag_data_get called\n");                            
    GtkIconView *icon_view = GTK_ICON_VIEW(widget);
    GList *selected_items = gtk_icon_view_get_selected_items(icon_view);

    if (selected_items == NULL) {
        // Nenhum item selecionado, nada para enviar
        return;
    }

    GtkTreePath *path = (GtkTreePath *)selected_items->data;

    GtkTreeModel *model = gtk_icon_view_get_model(icon_view);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        char *item_name = NULL;
        gtk_tree_model_get(model, &iter, 1, &item_name, -1);

        if (item_name) {
            // Define o texto do item arrastado na seleção
            gtk_selection_data_set_text(selection_data, item_name, -1);
            // Não dá g_free(item_name) aqui porque é obtido direto do modelo
            // Se seu gtk_tree_model_get aloca, descomente a linha abaixo:
            // g_free(item_name);
        }
    }

    g_list_free_full(selected_items, (GDestroyNotify)gtk_tree_path_free);
}

void on_create_dir(GtkMenuItem *menuitem, gpointer user_data) {
    g_print("Criar diretório selecionado\n");
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Criar Diretório",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(menuitem))),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_OK", GTK_RESPONSE_ACCEPT,
                                                    "_Cancelar", GTK_RESPONSE_REJECT,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Nome do diretório");
    gtk_box_pack_start(GTK_BOX(content_area), entry, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *dir_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (fs_create_directory(dir_name) == 0) {
            update_ls();
        } else {
            g_print("Erro ao criar diretório: %s\n", dir_name);
        }
    }

    gtk_widget_destroy(dialog);
}

void on_create_file(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Criar Arquivo",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(menuitem))),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_Criar", GTK_RESPONSE_ACCEPT,
                                                    "_Cancelar", GTK_RESPONSE_REJECT,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Nome do arquivo");

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, 400, 200);

    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    gtk_box_pack_start(GTK_BOX(content_area), entry, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(content_area), scrolled_window, TRUE, TRUE, 5);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *filename = gtk_entry_get_text(GTK_ENTRY(entry));
        
        if (strlen(filename) == 0) {
            fprintf(stderr, "Nome do arquivo não pode ser vazio.\n");
        } else {
            int item_type = fs_check_item_type(filename);
            if (item_type == TYPE_DIR) {
                fprintf(stderr, "Já existe um diretório com esse nome.\n");
            } else {
                GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
                GtkTextIter start, end;
                gtk_text_buffer_get_start_iter(buffer, &start);
                gtk_text_buffer_get_end_iter(buffer, &end);
                char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
                printf("Texto do arquivo: %s\n", text);
                printf("Nome do arquivo: %s\n", filename);
                if (fs_write_file(filename, text, ">") != 0) {
                    fprintf(stderr, "Erro ao criar o arquivo '%s'\n", filename);
                } else {
                    g_print("Arquivo '%s' criado com sucesso.\n", filename);
                    update_ls();
                }
        
                g_free(text);
            }
        }
    }

    gtk_widget_destroy(dialog);
}


void on_disk_usage(GtkMenuItem *menuitem, gpointer user_data) {
    g_print("Informações do disco selecionadas\n");
    DiskUsageInfo disk_info = fs_disk_free();
    if (disk_info.total_inodes == 0) {
        fprintf(stderr, "Erro ao obter informações do disco.\n");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Informações do Disco",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(menuitem))),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_OK", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    // Cabeçalho colunas
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(""), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Livres"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Usados"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Total"), 3, 0, 1, 1);

    // Linha i-nodes
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("I-nodes"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.free_inodes)), 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.used_inodes)), 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.total_inodes)), 3, 1, 1, 1);

    // Linha blocos
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Blocos"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.free_blocks)), 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.used_blocks)), 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.total_blocks)), 3, 2, 1, 1);

    // Linha espaço (KB)
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Espaço (KB)"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.free_kb)), 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.used_kb)), 2, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new(g_strdup_printf("%u", disk_info.total_kb)), 3, 3, 1, 1);

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

gboolean on_icon_view_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    g_print("[DEBUG] button_press_event: button %d\n", event->button);
    if (event->button == GDK_BUTTON_SECONDARY) {  // botão direito
        g_print("[DEBUG] Botão direito pressionado no IconView\n");
        GtkTreePath *path = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(widget), event->x, event->y);

        GtkMenu *menu = GTK_MENU(gtk_menu_new());

        if (path) {
            gtk_icon_view_unselect_all(GTK_ICON_VIEW(widget));
            gtk_icon_view_select_path(GTK_ICON_VIEW(widget), path);
            GtkTreeModel *model = gtk_icon_view_get_model(GTK_ICON_VIEW(widget));
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter(model, &iter, path)) {
                char *item_name;
                int item_type;
                gtk_tree_model_get(model, &iter,
                                   1, &item_name,
                                   2, &item_type,
                                   -1);

                GtkWidget *rename_item = gtk_menu_item_new_with_label("Renomear");
                g_signal_connect(rename_item, "activate", G_CALLBACK(on_rename_item), g_strdup(item_name));
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), rename_item);

                GtkWidget *delete_item = gtk_menu_item_new_with_label("Deletar");
                g_signal_connect(delete_item, "activate", G_CALLBACK(on_delete_item), g_strdup(item_name));
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);

                GtkWidget *properties_item = gtk_menu_item_new_with_label("Propriedades");
                g_signal_connect(properties_item, "activate", G_CALLBACK(on_item_properties), g_strdup(item_name));
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), properties_item);

                g_free(item_name);
            }
            gtk_tree_path_free(path);

        } else {
            GtkWidget *new_dir = gtk_menu_item_new_with_label("Criar Diretório");
            g_signal_connect(new_dir, "activate", G_CALLBACK(on_create_dir), NULL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_dir);

            GtkWidget *disk_info = gtk_menu_item_new_with_label("Informações do Disco");
            g_signal_connect(disk_info, "activate", G_CALLBACK(on_disk_usage), NULL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), disk_info);

            GtkWidget *create_file = gtk_menu_item_new_with_label("Criar Arquivo");
            g_signal_connect(create_file, "activate", G_CALLBACK(on_create_file), NULL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), create_file);
        }

        gtk_widget_show_all(GTK_WIDGET(menu));
        gtk_menu_popup_at_pointer(menu, (GdkEvent*) event);

        return TRUE;
    }
    return FALSE;
}

void on_item_activated(GtkIconView *icon_view, GtkTreePath *path, gpointer user_data) {
    GtkTreeModel *model = gtk_icon_view_get_model(icon_view);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        char *item_name;
        int item_type;

        gtk_tree_model_get(model, &iter,
                           1, &item_name,
                           2, &item_type,
                           -1);

        if (item_type == TYPE_DIR) {
            if (fs_change_directory(item_name) == 0) {
                update_ls();
            }
        } else {
            char *file_content = fs_read_file(item_name);
            if (file_content) {
                GtkWidget *dialog = gtk_dialog_new_with_buttons(
                    item_name,
                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(icon_view))),
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    "_Fechar", GTK_RESPONSE_CLOSE,
                    NULL
                );

                GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

                GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
                gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
                gtk_widget_set_size_request(scrolled_window, 400, 300);

                GtkWidget *text_view = gtk_text_view_new();
                gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
                gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);

                GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
                gtk_text_buffer_set_text(buffer, file_content, -1);

                gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
                gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);

                gtk_widget_show_all(dialog);
                gtk_dialog_run(GTK_DIALOG(dialog));
                gtk_widget_destroy(dialog);
                free(file_content);
            } else {
                fprintf(stderr, "Erro ao ler o arquivo '%s'\n", item_name);
            }
        }
        g_free(item_name);
    }
}

void create_interface(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Sistema de Arquivos");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    list_store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);

    icon_view = gtk_icon_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(icon_view), 0);
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(icon_view), 1);
    gtk_icon_view_set_columns(GTK_ICON_VIEW(icon_view), 0);  // Deixa o GTK escolher o número de colunas automaticamente
    gtk_icon_view_set_item_width(GTK_ICON_VIEW(icon_view), 100);
    gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(icon_view), 15);
    gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(icon_view), 15);

    // Drag and Drop
    const GtkTargetEntry drag_dest_targets[] = {
        { "text/uri-list", 0, 0 },
        { "text/plain", 0, 1 }
    };
    const GtkTargetEntry drag_source_targets[] = {
        { "text/plain", 0, 0 }
    };

    gtk_icon_view_enable_model_drag_source(GTK_ICON_VIEW(icon_view),
                                           GDK_BUTTON_PRIMARY,
                                           drag_source_targets, 1,
                                           GDK_ACTION_MOVE);

    gtk_icon_view_enable_model_drag_dest(GTK_ICON_VIEW(icon_view),
                                         drag_dest_targets, 2,
                                         GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect(icon_view, "item-activated", G_CALLBACK(on_item_activated), NULL);
    g_signal_connect(icon_view, "button-press-event", G_CALLBACK(on_icon_view_button_press), NULL);
    g_signal_connect(icon_view, "drag-data-received", G_CALLBACK(on_drag_data_received), NULL);
    g_signal_connect(icon_view, "drag-data-get", G_CALLBACK(on_drag_data_get), NULL);
    g_signal_connect(icon_view, "drag-begin", G_CALLBACK(on_drag_begin), NULL);

    // Coloca o IconView dentro de um ScrolledWindow para evitar o espaço vazio gigante
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), icon_view);

    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    update_ls();

    gtk_widget_show_all(window);
    gtk_main();
}
