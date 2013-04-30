using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.IO;

namespace FindDuplicates
{
    public partial class Form1 : Form
    {
        String [] jpg_avi;

        public Form1()
        {
            InitializeComponent();
            listView1.Columns.Add("File name", 300);
            last_dir_label.Text = "";
            
            jpg_avi = new String [6];
            jpg_avi[0] = ".avi";
            jpg_avi[1] = ".jpg";
            jpg_avi[2] = ".mpg";
            jpg_avi[3] = ".thm";
            jpg_avi[2] = ".mp4";
            jpg_avi[3] = ".3gp";
        }

        private String last_dir = "";

        private void iterate_dir(DirectoryInfo dir, MultiMap<string, FileInfo> files,
                                       String [] exts, bool ignore_hardcoded_exclusions)
        {
            if (!ignore_hardcoded_exclusions &&
                (
                    dir.Name.IndexOf("to") == 0 || dir.Name.IndexOf("кемер") == 0 ||
                    dir.Name.IndexOf("катя") == 0 || dir.Name.IndexOf("Фо") == 0 ||
                    dir.Name.IndexOf("стас") == 0)
                )
                return;

            try
            {
                foreach (FileInfo f in dir.GetFiles())
                {
                    foreach (String ext in exts)
                    {
                        if (String.Compare(f.Extension, ext, true) == 0)
                        {
                            files.Add(f.Name.ToLower(), f);
                        }
                    }
    //                if (f.Extension == ".jpg" || f.Extension == ".avi")
    //                    files.Add(f.Name, f);
                }
                foreach (DirectoryInfo di in dir.GetDirectories())
                {
                    iterate_dir(di, files, exts, ignore_hardcoded_exclusions);
                }
            }
            catch (UnauthorizedAccessException)
            {
                return;
            }
        }

        private void iterate_tree(String path, String[] exts)
        {
            iterate_tree(path, exts, true, false, null, false);
        }

        private MultiMap<string, FileInfo> iterate_tree(String path, String[] exts, bool ignore_hardcoded_exclusions, bool show_all, 
                                    MultiMap<string, FileInfo> files, bool do_not_alter_list)
        {
            if (path.Length == 0)
                return null;

            if (files == null)
                files = new MultiMap<string, FileInfo>();

            DirectoryInfo dir = new DirectoryInfo(path);
            iterate_dir(dir, files, exts, ignore_hardcoded_exclusions);

            if (do_not_alter_list)
                return files;

            listView1.Items.Clear();

            foreach (string key in files.Keys)
            {
                if (show_all || files[key].Count > 1)
                {
                    MultiMap<long, FileInfo> groupped = new MultiMap<long, FileInfo>();

                    foreach (FileInfo f in files[key])
                    {
                        groupped.Add(f.Length, f);
                    }
                    foreach (long len in groupped.Keys)
                    {
                        if (show_all || groupped[len].Count > 1)
                        {
                            foreach (FileInfo f in groupped[len])
                            {
                                listView1.Items.Add(f.FullName + " " + f.Length);
                            }
                            listView1.Items.Add("");
                        }
                    }
                }
            }

            return files;
        }

        private void button1_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog dlg = new FolderBrowserDialog();
            dlg.ShowNewFolderButton = false;
            if (dlg.ShowDialog(this) == DialogResult.OK)
            {
                last_dir = dlg.SelectedPath;
                last_dir_label.Text = last_dir;
                iterate_tree(last_dir, jpg_avi);
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            iterate_tree(last_dir, jpg_avi);
        }

        private void scan_for_Click(object sender, EventArgs e)
        {
            String [] ex = new String[1];
            ex[0] = ".avi";
            MultiMap<string, FileInfo> files = iterate_tree("c:\\", ex, true, true, null, true);
            iterate_tree("d:\\", ex, true, true, files, false);
        }

        private void SaveList_Click(object sender, EventArgs e)
        {
            SaveFileDialog dlg = new SaveFileDialog();
            if (dlg.ShowDialog(this) == DialogResult.OK)
            {
                Stream f = dlg.OpenFile();
                StreamWriter w = new StreamWriter(f, Encoding.GetEncoding(1251));
                foreach (ListViewItem i in listView1.Items)
                {
                    w.WriteLine(i.Text);
                }
                w.Flush();
                f.Close();
            }
        }

        private void listView1_MouseUp(object sender, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Right)
                return;

            if (!(sender is System.Windows.Forms.ListView))
                return;

            System.Windows.Forms.ListView list = (System.Windows.Forms.ListView)sender;

            if (list == null)
                return; // ???

            ListViewHitTestInfo info = list.HitTest(e.X, e.Y);

            if (info.Item == null)
                return;

            string dir = info.Item.Text;

            int p = dir.LastIndexOf('\\');

            if (p == -1)
                return;

            dir = dir.Substring(0, p);

            SaveFileDialog dlg = new SaveFileDialog();
            if (dlg.ShowDialog(this) == DialogResult.OK)
            {
                Stream f = dlg.OpenFile();
                StreamWriter w = new StreamWriter(f, Encoding.GetEncoding(1251));
                foreach (ListViewItem i in list.Items)
                {
                    string dir1 = i.Text;
                    int p1 = dir1.LastIndexOf('\\');
                    if (p1 != -1 && dir == dir1.Substring(0, p1))
                    {
                        int p2 = dir1.LastIndexOf(' ');
                        if (p2 != -1)
                        {
                            w.WriteLine("del " + i.Text.Substring(0, p2));
                        }
                    }
                }
                w.Flush();
                f.Close();
            }
        }
    }
}