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
            
            jpg_avi = new String [2];
            jpg_avi[0] = ".avi";
            jpg_avi[1] = ".jpg";
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
            iterate_tree(path, exts, false, false, null, false);
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
                    string s = "";
                    foreach (FileInfo f in files[key])
                    {
                        listView1.Items.Add(f.FullName);
                        s += f.FullName + " " + f.Length + "\n";
                    }
                    listView1.Items.Add("");
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
                f.Close();
            }
        }
    }
}