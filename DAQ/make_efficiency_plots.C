// make_efficiency_plots.C
// Draw all AnalysisEfficiency histograms from a Corryvreckan ROOT file and save as PNG.
// Usage: root -l -b -q 'make_efficiency_plots.C("Analysis_runXXX.root","output/efficiency/efficiency_runXXX/")'

#include <TFile.h>
#include <TDirectoryFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TProfile2D.h>
#include <TEfficiency.h>
#include <TStyle.h>
#include <TList.h>
#include <TKey.h>
#include <TString.h>
#include <iostream>
#include <string>

void drawAndSave(TCanvas* c, const std::string& outdir, const std::string& detName,
                 const std::string& histName, TObject* obj) {
    if(!obj) return;
    c->Clear();
    c->SetLogz(0);
    gPad->SetRightMargin(0.05f);

    std::string drawOpt = "";
    bool is2D = false;

    if(auto* te = dynamic_cast<TEfficiency*>(obj)) {
        int dim = te->GetDimension();
        if(dim == 2) {
            drawOpt = "colz";
            is2D = true;
            gPad->SetRightMargin(0.15f);
        } else {
            drawOpt = "AP";
        }
        te->SetTitle(histName.c_str());
        te->Draw(drawOpt.c_str());
    } else if(auto* tp = dynamic_cast<TProfile2D*>(obj)) {
        is2D = true;
        drawOpt = "colz";
        gPad->SetRightMargin(0.15f);
        tp->Draw(drawOpt.c_str());
    } else if(auto* h2 = dynamic_cast<TH2*>(obj)) {
        is2D = true;
        drawOpt = "colz";
        gPad->SetRightMargin(0.15f);
        h2->Draw(drawOpt.c_str());
    } else if(auto* h1 = dynamic_cast<TH1*>(obj)) {
        h1->SetLineColor(kBlack);
        h1->SetLineWidth(2);
        h1->Draw("HIST");
    } else {
        return;
    }

    c->Update();
    std::string outpath = outdir + detName + "_" + histName + ".png";
    c->Print(outpath.c_str());
    std::cout << "[SAVED] " << outpath << std::endl;
}

void processDUTDir(TCanvas* c, TDirectoryFile* detDir, const std::string& detName,
                   const std::string& outdir) {
    TList* keys = detDir->GetListOfKeys();
    for(auto* k : *keys) {
        TKey* key = dynamic_cast<TKey*>(k);
        if(!key) continue;

        std::string name = key->GetName();
        std::string cls  = key->GetClassName();

        // Recurse into subdirectories (inpixelROI, fake_rate, ...)
        if(cls.find("TDirectory") != std::string::npos) {
            TDirectoryFile* subDir = dynamic_cast<TDirectoryFile*>(detDir->Get(name.c_str()));
            if(!subDir) continue;
            TList* subKeys = subDir->GetListOfKeys();
            for(auto* sk : *subKeys) {
                TKey* skey = dynamic_cast<TKey*>(sk);
                if(!skey) continue;
                TObject* obj = skey->ReadObj();
                drawAndSave(c, outdir, detName, name + "_" + skey->GetName(), obj);
            }
            continue;
        }

        TObject* obj = key->ReadObj();
        drawAndSave(c, outdir, detName, name, obj);
    }
}

void make_efficiency_plots(const char* rootfile, const char* outdir_c) {
    gStyle->SetPalette(kBird);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetHistLineWidth(2);

    std::string outdir(outdir_c);
    if(outdir.back() != '/') outdir += '/';

    TFile* f = TFile::Open(rootfile, "READ");
    if(!f || f->IsZombie()) {
        std::cerr << "[ERROR] Cannot open " << rootfile << std::endl;
        return;
    }

    TDirectoryFile* effDir = dynamic_cast<TDirectoryFile*>(f->Get("AnalysisEfficiency"));
    if(!effDir) {
        std::cerr << "[ERROR] AnalysisEfficiency directory not found in " << rootfile << std::endl;
        f->Close();
        return;
    }

    TCanvas* c = new TCanvas("c_eff", "Efficiency", 900, 700);
    c->SetMargin(0.12f, 0.05f, 0.12f, 0.08f);
    c->SetGridx();
    c->SetGridy();

    TList* detKeys = effDir->GetListOfKeys();
    for(auto* k : *detKeys) {
        TKey* key = dynamic_cast<TKey*>(k);
        if(!key) continue;
        std::string cls = key->GetClassName();
        if(cls.find("TDirectory") == std::string::npos) continue;

        std::string detName = key->GetName();
        TDirectoryFile* detDir = dynamic_cast<TDirectoryFile*>(effDir->Get(detName.c_str()));
        if(!detDir) continue;

        std::cout << "[INFO] Processing DUT: " << detName << std::endl;
        processDUTDir(c, detDir, detName, outdir);
    }

    delete c;
    f->Close();
    std::cout << "[DONE] All efficiency plots saved to " << outdir << std::endl;
}
