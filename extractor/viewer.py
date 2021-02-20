#!/usr/bin/env python3
import archive
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk
from gi.repository import GdkPixbuf
from gi.repository import Gio
from gi.repository import GLib

def mime_pixbuf(mimetype):
    icon = Gio.content_type_get_icon(mimetype)
    if isinstance(icon, Gio.LoadableIcon):
        pass
    if isinstance(icon, Gio.EmblemedIcon):
        pass
    if isinstance(icon, Gio.ThemedIcon):
        icon_theme = Gtk.IconTheme.get_default()
        icon_info = icon_theme.choose_icon(icon.get_names(), size=0, flags=0)
        return icon_info.load_icon()

class ArchiveWindow(Gtk.Window):
    def __init__(self, filename, ar):
        Gtk.Window.__init__(self, title="Archive Viewer")
        self.box = Gtk.Box(spacing=6)

        store = Gtk.TreeStore(GdkPixbuf.Pixbuf, str, object)
        root_it = store.append(None, [mime_pixbuf("application/x-archive"), filename, None])
        images_it = store.append(root_it, [mime_pixbuf("image/generic"), "Images", None])
        for ix, image in enumerate(ar.images):
            store.append(images_it, [mime_pixbuf("image/generic"), f"#{ix}", image])
        store.append(root_it, [mime_pixbuf("text/plain"), "Palettes", None])
        store.append(root_it, [mime_pixbuf("text/plain"), "Animations", None])
        store.append(root_it, [mime_pixbuf("text/plain"), "Strings", None])
        store.append(root_it, [mime_pixbuf("audio/generic"), "Audio", None])

        tree = Gtk.TreeView(model=store)
        column = Gtk.TreeViewColumn()
        icon_renderer = Gtk.CellRendererPixbuf()
        column.pack_start(icon_renderer, True)
        column.add_attribute(icon_renderer, "pixbuf", 0)
        label_renderer = Gtk.CellRendererText()
        column.pack_end(label_renderer, True)
        column.add_attribute(label_renderer, "text", 1)
        tree.append_column(column)
        tree.get_selection().connect("changed", self.selection_changed)

        treescroll = Gtk.ScrolledWindow()
        treescroll.add(tree)
        self.box.pack_start(treescroll, True, True, 0)

        self.img = None
        self.add(self.box)

    def selection_changed(self, selection):
        model, iter = selection.get_selected()
        if iter is not None:
            (_0, _1, entry) = model[iter]
            if isinstance(entry, archive.Image):
                if self.img:
                    self.box.remove(self.img)
                pdata = bytearray()
                for y in range(0, entry.height):
                    for x in range(0, entry.width):
                        r,g,b,a = entry.rgba(x, y)
                        pdata.append(r)
                        pdata.append(g)
                        pdata.append(b)
                        pdata.append(a)
                pbuf = GdkPixbuf.Pixbuf.new_from_bytes (
                    data=GLib.Bytes.new(bytes(pdata)),
                    colorspace=GdkPixbuf.Colorspace.RGB,
                    has_alpha=True,
                    bits_per_sample=8,
                    width=entry.width,
                    height=entry.height,
                    rowstride=entry.width * 4
                )
                self.img = Gtk.Image.new_from_pixbuf(pbuf)
                self.box.pack_end(self.img, True, True, 20)
                self.box.show_all()

def view(ar):
    win = ArchiveWindow("foo", ar)
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()
